# Portia Tier Switching - Quick Reference

## Initialization

```c
#include "portia/nimcp_portia_tier_switch.h"

// Default config (auto-switching enabled)
tier_switch_config_t config = portia_tier_switch_default_config();
portia_tier_switch_t switcher = portia_tier_switch_init(&config);

// Custom config
config.memory_high_threshold = 80.0f;
config.hysteresis_ms = 60000;
portia_tier_switch_t switcher = portia_tier_switch_init(&config);
```

## Automatic Tier Management

```c
// Auto-switching enabled by default
// System monitors conditions every 5 seconds (configurable)

// Disable auto-switching
portia_tier_switch_set_auto_switch(switcher, false);

// Re-enable auto-switching
portia_tier_switch_set_auto_switch(switcher, true);
```

## Manual Tier Control

```c
// Request specific tier
portia_tier_switch_request(switcher, PLATFORM_TIER_MEDIUM);

// Emergency downgrade to minimal
portia_tier_switch_emergency_downgrade(switcher);

// Check if upgrade is safe
if (portia_tier_switch_can_upgrade(switcher, PLATFORM_TIER_FULL)) {
    portia_tier_switch_request(switcher, PLATFORM_TIER_FULL);
}

// Check if downgrade needed
platform_tier_t target;
tier_switch_trigger_t trigger;
if (portia_tier_switch_can_downgrade(switcher, &target, &trigger)) {
    printf("Downgrade needed: %s\n", portia_tier_switch_trigger_name(trigger));
}
```

## State Queries

```c
// Get current tier
platform_tier_t tier = portia_tier_switch_get_current_tier(switcher);

// Get complete state
tier_switch_state_t state;
portia_tier_switch_get_state(switcher, &state);

// Check if transitioning
if (portia_tier_switch_is_transitioning(switcher)) {
    printf("Transition in progress\n");
}

// Get statistics
uint32_t total, upgrades, downgrades, failed;
portia_tier_switch_get_statistics(switcher, &total, &upgrades, &downgrades, &failed);
```

## Event Callbacks

```c
void my_callback(const tier_switch_event_t* event, void* user_data) {
    printf("Tier: %s -> %s\n",
           platform_tier_get_name(event->old_tier),
           platform_tier_get_name(event->new_tier));
}

portia_tier_switch_set_callback(switcher, my_callback, NULL);
```

## Configuration

```c
// Update configuration at runtime
tier_switch_config_t new_config = portia_tier_switch_default_config();
new_config.memory_high_threshold = 90.0f;
portia_tier_switch_update_config(switcher, &new_config);
```

## Debug Output

```c
// Print current state to stdout
portia_tier_switch_print_state(switcher);
```

## Cleanup

```c
// Clean shutdown (stops monitoring thread, frees resources)
portia_tier_switch_shutdown(switcher);
```

## Tier Switch Triggers

- `TIER_SWITCH_TRIGGER_MEMORY_PRESSURE` - RAM usage high
- `TIER_SWITCH_TRIGGER_THERMAL_THROTTLE` - CPU too hot
- `TIER_SWITCH_TRIGGER_BATTERY_LOW` - Battery critical
- `TIER_SWITCH_TRIGGER_LOAD_SPIKE` - CPU saturated
- `TIER_SWITCH_TRIGGER_USER_REQUEST` - Manual switch
- `TIER_SWITCH_TRIGGER_PERFORMANCE_GOAL` - Latency target missed
- `TIER_SWITCH_TRIGGER_RESOURCE_AVAILABLE` - Resources freed
- `TIER_SWITCH_TRIGGER_INIT` - Initial selection

## Default Thresholds

| Metric | Downgrade | Upgrade | Critical |
|--------|-----------|---------|----------|
| Memory | 85% | < 60% | 95% |
| Temperature | 80°C | < 65°C | N/A |
| Battery | 20% | > 50% | N/A |
| CPU Load | 90% | < 50% | N/A |

## Default Timing

- **Hysteresis:** 30 seconds
- **Evaluation Interval:** 5 seconds
- **Transition Timeout:** 60 seconds
- **Module Shutdown Timeout:** 10 seconds

## Thread Safety

All API functions are thread-safe. Internal state is protected by mutexes.

## Performance

- Evaluation: < 1ms (with cached metrics)
- Transition: < 100ms (without modules)
- Background Thread: < 0.1% CPU
- Memory: < 100KB overhead
