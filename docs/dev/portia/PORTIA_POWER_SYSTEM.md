# Portia Power-Aware Tier System

## Overview

The **Portia Power System** is NIMCP's battery-aware resource management framework for mobile and embedded platforms. Named after *Portia fimbriata*, the jumping spider that optimizes energy expenditure based on metabolic state, this system automatically adapts computational intensity to power availability.

## Biological Inspiration

Just as Portia spiders switch between high-energy hunting and low-energy waiting based on hunger, NIMCP adapts its cognitive architecture based on battery state:

- **High Battery**: Full cognitive capabilities (like well-fed Portia)
- **Medium Battery**: Reduced but functional cognition
- **Low Battery**: Essential functions only (survival mode)
- **Critical Battery**: Minimal reactive processing

## Power Profiles

### PERFORMANCE (>80% or AC Power)
- **Max Resources**: 100% of platform tier capability
- **All Modules**: Full cognitive suite enabled
- **Update Rate**: Maximum (100 Hz)
- **Use Cases**: Research, development, demanding inference

### BALANCED (40-80% Battery)
- **Scaled Resources**: 75% neurons, 80% update rate
- **Core Modules**: Working memory, attention, emotions, reasoning
- **Use Cases**: Normal operation, standard inference

### SAVER (20-40% Battery)
- **Reduced Resources**: 50% neurons, 50% update rate
- **Essential Modules**: Basic attention, working memory, emotions
- **Disabled**: Curiosity, meta-learning, introspection
- **Use Cases**: Extended battery life needed

### CRITICAL (10-20% Battery)
- **Minimal Resources**: 25% neurons, 20% update rate
- **Critical Modules**: Attention, working memory, salience only
- **Use Cases**: Approaching shutdown, emergency operation

### EMERGENCY (<10% Battery)
- **Survival Mode**: 10% neurons, 5% update rate
- **Reactive Only**: Basic attention for critical events
- **Use Cases**: Final minutes before shutdown

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  PORTIA POWER MANAGER                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌────────────────┐     ┌──────────────────┐               │
│  │  Battery       │────>│  Profile         │               │
│  │  Monitoring    │     │  Determination   │               │
│  │  Thread        │     └──────────────────┘               │
│  └────────────────┘              │                          │
│         │                        │                          │
│         │ /sys/class/power_supply│                          │
│         │                        V                          │
│         │              ┌──────────────────┐                 │
│         └─────────────>│  Tier Config     │                 │
│                        │  Adaptation      │                 │
│                        └──────────────────┘                 │
│                                 │                           │
│                                 V                           │
│                        ┌──────────────────┐                 │
│                        │  Bio-Async       │                 │
│                        │  Events          │                 │
│                        └──────────────────┘                 │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Linux Integration

The system reads battery information from sysfs:

```bash
/sys/class/power_supply/BAT0/
├── status           # Charging/Discharging/Full
├── capacity         # Battery percentage (0-100)
├── power_now        # Current power draw (μW)
├── charge_now       # Current charge (μAh)
├── charge_full      # Max charge (μAh)
├── temp             # Temperature (0.1°C)
└── health           # Battery health status
```

## Usage Examples

### Basic Initialization

```c
#include "portia/nimcp_portia_power.h"

// Initialize with defaults
portia_power_config_t config = portia_power_default_config();
portia_power_manager_t pm = portia_power_init(&config);

// Get current status
power_status_t status;
portia_power_get_status(pm, &status);
printf("Battery: %.1f%%\n", status.battery_level_pct);

// Cleanup
portia_power_shutdown(pm);
```

### Automatic Profile Adaptation

```c
// Enable automatic profile adjustment
config.auto_adjust_profile = true;
config.poll_interval_ms = 5000;  // Check every 5 seconds

portia_power_manager_t pm = portia_power_init(&config);

// Profile automatically changes based on battery level
power_profile_t profile = portia_power_get_profile(pm);
printf("Current profile: %s\n", portia_power_get_profile_name(profile));
```

### Manual Profile Control

```c
// Force specific profile (e.g., for testing)
portia_power_set_profile(pm, POWER_PROFILE_SAVER);

// Get tier configuration for profile
platform_tier_t tier = platform_tier_detect();
power_tier_config_t tier_config = portia_power_get_tier_config(pm, tier, -1);

printf("Max neurons: %u\n", tier_config.max_neurons);
printf("Processing rate: %.1f Hz\n", tier_config.processing_rate_hz);
```

### Runtime Estimation

```c
// Estimate remaining runtime
float runtime_s = portia_power_estimate_runtime(pm, 0.9f);  // 90% safety margin

if (runtime_s > 0.0f) {
    int hours = (int)(runtime_s / 3600.0f);
    int minutes = (int)((runtime_s - hours * 3600) / 60.0f);
    printf("Estimated runtime: %dh %dm\n", hours, minutes);
} else {
    printf("Runtime: Unlimited (AC power)\n");
}
```

### Bio-Async Power Events

```c
// Enable bio-async notifications
config.enable_bio_async_events = true;

// Events are sent on:
// - POWER_EVENT_PROFILE_CHANGE: Profile changed
// - POWER_EVENT_BATTERY_LOW: <20% battery
// - POWER_EVENT_BATTERY_CRITICAL: <10% battery
// - POWER_EVENT_CHARGING_STARTED: Charging began
// - POWER_EVENT_THERMAL_WARNING: Battery too hot

// Subscribe to events in your module
void handle_power_event(bio_msg_power_event_t* msg) {
    if (msg->event_type == POWER_EVENT_BATTERY_LOW) {
        // Reduce activity
        reduce_cognitive_load();
    }
}
```

## Configuration Options

```c
typedef struct {
    // Monitoring
    uint32_t poll_interval_ms;          // Status polling interval (default: 5000)
    bool auto_adjust_profile;           // Auto-adjust on battery change (default: true)
    bool enable_bio_async_events;       // Send bio-async notifications (default: true)

    // Battery thresholds (percentage)
    float performance_threshold;        // Enter performance mode (default: 80%)
    float balanced_threshold;           // Enter balanced mode (default: 40%)
    float saver_threshold;              // Enter saver mode (default: 20%)
    float critical_threshold;           // Enter critical mode (default: 10%)

    // Thermal limits
    float max_safe_temp_c;              // Max safe temperature (default: 45°C)
    float thermal_throttle_temp_c;      // Start throttling (default: 40°C)

    // Runtime estimation
    float discharge_history_s;          // History window (default: 60s)

    // Platform detection
    const char* battery_path;           // Override battery sysfs path
    bool force_battery_mode;            // Pretend always on battery
} portia_power_config_t;
```

## Integration with Brain

```c
// Detect platform tier
platform_tier_t tier = platform_tier_detect();

// Initialize power monitoring
portia_power_manager_t pm = portia_power_init(NULL);

// Get power-adjusted configuration
power_tier_config_t config = portia_power_get_tier_config(pm, tier, -1);

// Create brain with adjusted limits
brain_t* brain = brain_create(config.max_neurons, config.max_neurons / 2);

// Periodically check and adjust
while (running) {
    power_profile_t profile = portia_power_get_profile(pm);
    if (profile <= POWER_PROFILE_SAVER) {
        // Reduce update rate
        brain_set_update_rate(brain, config.processing_rate_hz);
    }
}
```

## Statistics

Track power management metrics:

```c
portia_power_stats_t stats;
portia_power_get_stats(pm, &stats);

printf("Samples taken: %llu\n", stats.samples_taken);
printf("Profile changes: %llu\n", stats.profile_changes);
printf("Avg battery: %.1f%%\n", stats.avg_battery_level);
printf("Avg discharge: %.1f mW\n", stats.avg_discharge_rate_mw);
printf("Thermal throttles: %u\n", stats.thermal_throttles);
```

## Performance Impact

- **Polling Overhead**: ~0.1% CPU (5s interval)
- **Memory**: ~1KB per manager instance
- **Battery Detection**: <1ms per sample
- **Thread-Safe**: Lock-free reads, mutex-protected writes

## Platform Support

### Linux
- **Full Support**: Battery monitoring via sysfs
- **Tested**: Ubuntu, Debian, Raspberry Pi OS

### macOS
- **Planned**: IOKit power source API

### Windows
- **Planned**: Power Management API

### Embedded
- **Custom**: Implement battery_read_status() hook

## Best Practices

1. **Initialize Early**: Call `portia_power_init()` at startup
2. **Use Auto-Adjustment**: Enable `auto_adjust_profile` for most cases
3. **Monitor Events**: Subscribe to bio-async power events
4. **Test Profiles**: Test with all profiles during development
5. **Respect Limits**: Don't exceed tier config limits
6. **Handle Thermal**: Monitor temperature, throttle if needed
7. **Estimate Runtime**: Use estimates for long-running operations

## Debugging

Enable detailed logging:

```c
nimcp_log_config_t log_config = nimcp_log_default_config();
log_config.level = LOG_LEVEL_DEBUG;
nimcp_log_init(&log_config);
```

Logs show:
- Battery detection
- Profile changes
- Tier config scaling
- Event broadcasts

## Examples

See:
- `examples/portia_power_demo.c` - Full demonstration
- `test/unit/portia/test_portia_power.cpp` - Unit tests

## References

1. Portia Spider Research: https://en.wikipedia.org/wiki/Portia_(spider)
2. Linux Power Supply API: `/Documentation/power/power_supply_class.txt`
3. Platform Tier System: `docs/PLATFORM_TIER_SYSTEM.md`

## Authors

NIMCP Development Team

## License

Part of the NIMCP project
