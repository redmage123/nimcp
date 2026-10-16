# Signal Handler Quick Reference Guide

## Overview

Signal handler with integrated fault tolerance components including checkpoint saving, health monitoring, and recovery management.

## Core Setup

```c
#include "utils/signal/nimcp_signal_handler.h"

// 1. Install with default config
signal_handler_install(NULL);

// 2. Register brain for checkpoint support
brain_t brain = brain_create("my_brain", BRAIN_SIZE_MEDIUM);
signal_handler_register_brain(brain);

// 3. Configure checkpoint retention
signal_handler_set_checkpoint_retention(5);  // Keep last 5 checkpoints

// 4. Enable auto-recovery
signal_handler_set_auto_recovery(true);
signal_handler_set_max_recovery_attempts(3);
```

## Health Monitoring

```c
// Get comprehensive health status
signal_health_info_t health = signal_handler_get_health_status();

// Check status
if (health.status >= SIGNAL_HEALTH_DEGRADED) {
    printf("Warning: System health degraded\n");
}

// Monitor recovery effectiveness
printf("Recovery success rate: %.1f%%\n", health.recovery_success_rate);
printf("Total signals: %llu\n", health.total_signals);
printf("Checkpoint saves: %llu\n", health.checkpoint_saves);
```

## Checkpoint Management

```c
// Enable checkpoint saving on crash
signal_handler_config_t config = signal_handler_default_config();
config.enable_checkpoint_save = true;
config.checkpoint_path = "/var/checkpoints";
signal_handler_install(&config);

// Manually save checkpoint
if (signal_handler_checkpoint_save(NULL)) {
    printf("Checkpoint saved successfully\n");
}

// Query checkpoint status
int count = signal_handler_get_checkpoint_count();
printf("Available checkpoints: %d\n", count);

// Configure retention
signal_handler_set_checkpoint_retention(10);  // Keep 10 checkpoints
signal_handler_set_checkpoint_retention(0);   // Unlimited
```

## Recovery Control

```c
// Enable/disable automatic recovery
signal_handler_set_auto_recovery(true);   // Enable
signal_handler_set_auto_recovery(false);  // Disable for critical sections

// Verify recovery status
if (signal_handler_is_auto_recovery_enabled()) {
    printf("Auto-recovery is active\n");
}

// Limit recovery attempts
signal_handler_set_max_recovery_attempts(5);  // Max 5 attempts
signal_handler_set_max_recovery_attempts(0);  // Unlimited
```

## Signal Statistics

```c
// Get all signal counts
signal_handler_stats_t stats = signal_handler_get_stats();

printf("SIGSEGV: %llu\n", stats.sigsegv_count);
printf("SIGABRT: %llu\n", stats.sigabrt_count);
printf("SIGBUS:  %llu\n", stats.sigbus_count);
printf("SIGFPE:  %llu\n", stats.sigfpe_count);
printf("Recoveries: %llu\n", stats.recoveries);
printf("Fatal crashes: %llu\n", stats.fatal_crashes);

// Reset statistics
signal_handler_reset_stats();
```

## Signal Names

```c
// Get human-readable signal names
const char* name = signal_handler_get_signal_name(SIGSEGV);
printf("Signal: %s\n", name);  // "SIGSEGV"

// Last signal received
int last_sig = signal_handler_get_last_signal();
```

## Configuration Presets

### Default Configuration
```c
signal_handler_config_t config = signal_handler_default_config();
// SIGSEGV/SIGABRT → LOG_SHUTDOWN
// SIGFPE → LOG_CONTINUE (try to recover)
// SIGHUP → LOG_CONTINUE (reload config)
// Stack trace: ENABLED
// Checkpoint: DISABLED by default (enable if needed)
```

### Production Configuration
```c
signal_handler_config_t config = signal_handler_default_config();
config.enable_checkpoint_save = true;
config.checkpoint_path = "/var/lib/app/checkpoints";
config.enable_stack_trace = true;
config.sigsegv_mode = SIGNAL_MODE_LOG_SHUTDOWN;
signal_handler_install(&config);
```

### Development Configuration
```c
signal_handler_config_t config = signal_handler_default_config();
config.enable_state_dump = true;  // Dump internal state on crash
config.enable_stack_trace = true;
signal_handler_install(&config);
```

## Cleanup

```c
// Unregister brain
signal_handler_unregister_brain();

// Uninstall signal handlers (restore defaults)
signal_handler_uninstall();
```

## Health Status Levels

| Level | Meaning | Action |
|-------|---------|--------|
| HEALTHY | All systems operational | Normal operation |
| DEGRADED | Some issues detected | Monitor closely |
| COMPROMISED | Multiple issues, stability at risk | Prepare for failover |
| CRITICAL | Critical issues | Immediate intervention needed |
| UNKNOWN | Status cannot be determined | Investigate |

## Common Patterns

### Pattern 1: Safe Operation with Checkpoint

```c
// Save checkpoint before risky operation
signal_handler_checkpoint_save(NULL);

// Perform risky operation
risky_computation();

// Check health after
signal_health_info_t health = signal_handler_get_health_status();
if (health.status >= SIGNAL_HEALTH_CRITICAL) {
    // Handle critical state
}
```

### Pattern 2: Disable Recovery for Critical Section

```c
// Disable recovery during initialization
signal_handler_set_auto_recovery(false);

// Critical initialization code
initialize_critical_system();

// Re-enable recovery
signal_handler_set_auto_recovery(true);
```

### Pattern 3: Monitor Health Periodically

```c
void health_monitor_thread(void) {
    while (1) {
        signal_health_info_t health = signal_handler_get_health_status();

        if (health.status >= SIGNAL_HEALTH_DEGRADED) {
            log_alert("System health: %d", health.status);
        }

        sleep(60);  // Check every minute
    }
}
```

### Pattern 4: Checkpoint Rotation

```c
// Automatic checkpoint rotation
signal_handler_set_checkpoint_retention(5);

// Periodically checkpoint
void periodic_checkpoint(void) {
    if (signal_handler_checkpoint_save(NULL)) {
        printf("Checkpoint %d saved\n",
               signal_handler_get_checkpoint_count());
    }
}
```

## Signals Handled

| Signal | Default | Configurable |
|--------|---------|--------------|
| SIGSEGV | LOG_SHUTDOWN | Yes |
| SIGABRT | LOG_SHUTDOWN | Yes |
| SIGBUS | LOG_SHUTDOWN | Yes |
| SIGFPE | LOG_CONTINUE | Yes |
| SIGILL | LOG_SHUTDOWN | Yes |
| SIGTERM | LOG_SHUTDOWN | Yes |
| SIGINT | LOG_SHUTDOWN | Yes |
| SIGHUP | LOG_CONTINUE (reload) | Yes |

## Best Practices

1. **Always register brain early**
   ```c
   brain_t brain = brain_create(...);
   signal_handler_register_brain(brain);
   ```

2. **Enable checkpoint save in production**
   ```c
   config.enable_checkpoint_save = true;
   ```

3. **Monitor health regularly**
   ```c
   signal_health_info_t health = signal_handler_get_health_status();
   // Check health.status and take action
   ```

4. **Configure appropriate retention**
   ```c
   signal_handler_set_checkpoint_retention(10);  // Balance space vs. recovery
   ```

5. **Test recovery paths**
   ```c
   // Disable recovery to test graceful shutdown
   signal_handler_set_auto_recovery(false);
   ```

## Troubleshooting

**Q: Checkpoints not being saved?**
- Verify `enable_checkpoint_save = true` in config
- Check directory permissions for `checkpoint_path`
- Ensure brain is registered with `signal_handler_register_brain()`

**Q: Recovery not working?**
- Check if `signal_handler_is_auto_recovery_enabled()` returns true
- Verify recovery attempts haven't exceeded limit
- Check health status with `signal_handler_get_health_status()`

**Q: Checkpoint directory too large?**
- Reduce retention limit: `signal_handler_set_checkpoint_retention(n)`
- Manually clean old checkpoints in directory

**Q: High signal counts?**
- Monitor health: `signal_handler_get_health_status()`
- Increase max recovery attempts if needed
- Review error logs for root cause

## Testing

```bash
# Build tests
cd /home/bbrelin/nimcp/build
cmake ..
make test_signal_handler

# Run tests
./test/unit/utils/signal/test_signal_handler

# Run specific test
./test/unit/utils/signal/test_signal_handler \
    --gtest_filter=SignalHandlerTest.GetSignalStats
```

## API Reference

### Installation
- `signal_handler_install(config)` - Install handlers
- `signal_handler_uninstall()` - Restore defaults

### Brain Registration
- `signal_handler_register_brain(brain)` - Register for checkpoint
- `signal_handler_unregister_brain()` - Unregister

### Statistics
- `signal_handler_get_stats()` - Get all signal counts
- `signal_handler_reset_stats()` - Reset counters
- `signal_handler_get_last_signal()` - Last signal number
- `signal_handler_get_signal_name(sig)` - Human-readable name

### Health Monitoring
- `signal_handler_get_health_status()` - Comprehensive status

### Checkpointing
- `signal_handler_checkpoint_save(path)` - Save checkpoint
- `signal_handler_set_checkpoint_retention(max)` - Retention policy
- `signal_handler_get_checkpoint_count()` - Query count
- `signal_handler_force_checkpoint()` - Deprecated API

### Recovery
- `signal_handler_set_auto_recovery(enable)` - Control recovery
- `signal_handler_is_auto_recovery_enabled()` - Query status
- `signal_handler_set_max_recovery_attempts(max)` - Limit attempts

### Callbacks
- `signal_handler_set_crash_callback(callback)` - Fatal signal hook
- `signal_handler_set_reload_callback(callback)` - Config reload hook

## See Also

- Full documentation: `/home/bbrelin/nimcp/SIGNAL_HANDLER_INTEGRATION_REPORT.md`
- Header file: `/home/bbrelin/nimcp/src/utils/signal/nimcp_signal_handler.h`
- Implementation: `/home/bbrelin/nimcp/src/utils/signal/nimcp_signal_handler.c`
- Tests: `/home/bbrelin/nimcp/test/unit/utils/signal/test_signal_handler.cpp`
