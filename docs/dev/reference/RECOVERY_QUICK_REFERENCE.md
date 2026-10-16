# NIMCP Recovery System - Quick Reference

## Quick Start

### 1. Basic Recovery
```c
#include "utils/fault_tolerance/nimcp_recovery.h"

// Execute recovery for a specific failure
diagnostic_result_t diagnosis = {
    .signal = SIGFPE,
    .failure_type = "numeric_error",
    .severity = 6,
    .is_recoverable = true
};

recovery_result_t result = recovery_execute_strategy(brain, &diagnosis);
if (result.status == RECOVERY_SUCCESS) {
    printf("Recovered in %u us\n", result.time_us);
}
```

### 2. Auto-Healing
```c
// Automatic recovery without diagnosis
recovery_auto_heal(brain, NULL);
```

### 3. Retry with Backoff
```c
operation_t op = {
    .name = "operation",
    .execute = my_function,
    .context = my_data
};
recovery_retry_operation(brain, &op, 5);  // Retry 5 times
```

### 4. Circuit Breaker
```c
circuit_breaker_t* cb = circuit_breaker_create(5, 1000);

if (circuit_breaker_allow_operation(cb)) {
    if (perform_operation()) {
        circuit_breaker_record_success(cb);
    } else {
        circuit_breaker_record_failure(cb);
    }
}

circuit_breaker_destroy(cb);
```

---

## Recovery Tiers

| Tier | Speed | Examples |
|------|-------|----------|
| **IMMEDIATE** | <1ms | Clear NaN, Reset FPU |
| **TACTICAL** | <100ms | GC, Reload checkpoint |
| **STRATEGIC** | <1s | CPU fallback, Reduce model |
| **PREVENTIVE** | Variable | Auto-checkpoint, Increase memory |

---

## Common Failure Scenarios

### SIGSEGV (Segmentation Fault)
```c
diagnostic_result_t diag = {
    .signal = SIGSEGV,
    .failure_type = "segmentation_fault",
    .severity = 9,
    .is_recoverable = true
};
// → STRATEGIC: Reload checkpoint
```

### SIGFPE (Floating Point Exception)
```c
diagnostic_result_t diag = {
    .signal = SIGFPE,
    .failure_type = "numeric_error",
    .severity = 5,
    .is_recoverable = true
};
// → IMMEDIATE: Clear NaN + TACTICAL: Reduce LR
```

### Memory Exhaustion
```c
diagnostic_result_t diag = {
    .signal = SIGABRT,
    .failure_type = "memory_exhaustion",
    .severity = 7,
    .is_recoverable = true
};
// → TACTICAL: Trigger GC + Reduce batch
```

---

## API Reference

### Core Functions

```c
// Execute recovery strategy
recovery_result_t recovery_execute_strategy(brain_t brain, diagnostic_result_t* diagnosis);

// Retry operation with backoff
recovery_result_t recovery_retry_operation(brain_t brain, operation_t* op, uint32_t max_retries);

// Fallback to CPU
recovery_result_t recovery_fallback_cpu(brain_t brain);

// Rollback to checkpoint
recovery_result_t recovery_rollback_state(brain_t brain, const char* checkpoint);

// Auto-heal
bool recovery_auto_heal(brain_t brain, diagnostic_result_t* diagnosis);

// Adjust parameters
bool recovery_adjust_parameters(brain_t brain, adjustment_type_t type);
```

### Circuit Breaker Functions

```c
// Create circuit breaker
circuit_breaker_t* circuit_breaker_create(uint32_t failure_threshold, uint32_t timeout_ms);

// Destroy circuit breaker
void circuit_breaker_destroy(circuit_breaker_t* cb);

// Check if operation allowed
bool circuit_breaker_allow_operation(circuit_breaker_t* cb);

// Record success/failure
void circuit_breaker_record_success(circuit_breaker_t* cb);
void circuit_breaker_record_failure(circuit_breaker_t* cb);

// Reset circuit breaker
void circuit_breaker_reset(circuit_breaker_t* cb);

// Get state
circuit_state_t circuit_breaker_get_state(circuit_breaker_t* cb);
```

---

## Recovery Actions

### Immediate Actions
- `RECOVERY_ACTION_CLEAR_NAN` - Replace NaN/Inf with zeros
- `RECOVERY_ACTION_RESET_COUNTER` - Reset iteration counter
- `RECOVERY_ACTION_FLUSH_CACHE` - Clear temporary caches
- `RECOVERY_ACTION_RESET_FPU` - Reset FPU exception flags

### Tactical Actions
- `RECOVERY_ACTION_RELOAD_CHECKPOINT` - Rollback to checkpoint
- `RECOVERY_ACTION_REDUCE_LR` - Reduce learning rate 50%
- `RECOVERY_ACTION_REDUCE_BATCH` - Reduce batch size 50%
- `RECOVERY_ACTION_TRIGGER_GC` - Run garbage collection
- `RECOVERY_ACTION_RESTART_OP` - Retry with backoff

### Strategic Actions
- `RECOVERY_ACTION_FALLBACK_CPU` - Switch GPU → CPU
- `RECOVERY_ACTION_REDUCE_MODEL` - Reduce model complexity
- `RECOVERY_ACTION_REINIT_LAYER` - Reinitialize layer
- `RECOVERY_ACTION_EMERGENCY_SAVE` - Emergency checkpoint

### Preventive Actions
- `RECOVERY_ACTION_INCREASE_MEMORY` - Increase memory limits
- `RECOVERY_ACTION_COMPACT_MEMORY` - Defragment memory
- `RECOVERY_ACTION_ENABLE_CHECKS` - Enable validation
- `RECOVERY_ACTION_AUTO_CHECKPOINT` - Enable auto-save

---

## Parameter Adjustments

```c
// Reduce learning rate by 50%
recovery_adjust_parameters(brain, ADJUSTMENT_LEARNING_RATE);

// Reduce batch size by 50%
recovery_adjust_parameters(brain, ADJUSTMENT_BATCH_SIZE);

// Increase memory limit by 20%
recovery_adjust_parameters(brain, ADJUSTMENT_MEMORY_LIMIT);

// Double operation timeout
recovery_adjust_parameters(brain, ADJUSTMENT_TIMEOUT);

// Switch to float64 precision
recovery_adjust_parameters(brain, ADJUSTMENT_PRECISION);
```

---

## Recovery Status Codes

```c
RECOVERY_SUCCESS          // Recovery successful
RECOVERY_PARTIAL          // Partial recovery (degraded mode)
RECOVERY_FAILED           // Recovery failed
RECOVERY_NOT_APPLICABLE   // Strategy not applicable
RECOVERY_REQUIRES_RESTART // Requires process restart
```

---

## Circuit Breaker States

```
CIRCUIT_CLOSED      // Normal operation
CIRCUIT_OPEN        // Too many failures (blocking)
CIRCUIT_HALF_OPEN   // Testing recovery
```

---

## Best Practices

### 1. Always Check Recovery Results
```c
recovery_result_t result = recovery_execute_strategy(brain, &diagnosis);
if (result.status == RECOVERY_SUCCESS) {
    // Continue operation
} else if (result.status == RECOVERY_PARTIAL) {
    // Degraded mode - reduce functionality
} else {
    // Failed - consider restart
}
```

### 2. Use Circuit Breakers for Critical Operations
```c
circuit_breaker_t* cb = circuit_breaker_create(5, 1000);

// Wrap critical operations
if (circuit_breaker_allow_operation(cb)) {
    // Perform operation
}
```

### 3. Enable Auto-Healing for Production
```c
// In main loop
if (error_detected) {
    recovery_auto_heal(brain, NULL);
}
```

### 4. Configure Retry Backoff
```c
// For transient failures
operation_t op = { /* ... */ };
recovery_retry_operation(brain, &op, 5);  // 5 retries with exponential backoff
```

### 5. Monitor Recovery Statistics
```c
// Track recovery effectiveness
recovery_stats_t stats;
recovery_get_stats(&stats);
printf("Success rate: %.1f%%\n",
       100.0 * stats.successful_recoveries / stats.total_recoveries);
```

---

## Integration Examples

### Signal Handler Integration
```c
static void handle_fatal_signal(int sig) {
    diagnostic_result_t diagnosis = { .signal = sig };
    recovery_execute_strategy(g_brain, &diagnosis);
}
```

### Health Monitor Integration
```c
void check_health(brain_t brain, health_status_t* health) {
    if (health->memory_usage > 0.9f) {
        recovery_adjust_parameters(brain, ADJUSTMENT_BATCH_SIZE);
    }
    if (health->nan_count > 100) {
        recovery_auto_heal(brain, NULL);
    }
}
```

### Training Loop Integration
```c
for (int epoch = 0; epoch < num_epochs; epoch++) {
    if (!train_epoch(brain)) {
        // Auto-recover from training failure
        recovery_auto_heal(brain, NULL);
    }
}
```

---

## Troubleshooting

### Recovery Not Working
1. Check brain instance is valid
2. Verify diagnosis is populated
3. Ensure checkpoint exists for rollback
4. Check memory availability

### Circuit Breaker Always Open
1. Verify failure threshold is reasonable
2. Check timeout duration
3. Consider manual reset
4. Review failure causes

### Retry Not Succeeding
1. Increase max_retries
2. Check operation execute function
3. Verify context data
4. Review backoff timing

---

## Performance Tips

1. **Use Immediate tier** for hot paths (<1ms overhead)
2. **Batch parameter adjustments** to reduce overhead
3. **Cache circuit breakers** for repeated operations
4. **Monitor recovery stats** to optimize strategies
5. **Tune retry limits** based on failure patterns

---

## Files

- **Header**: `/home/bbrelin/nimcp/include/utils/fault_tolerance/nimcp_recovery.h`
- **Implementation**: `/home/bbrelin/nimcp/src/utils/fault_tolerance/nimcp_recovery.c`
- **Tests**: `/home/bbrelin/nimcp/test/unit/utils/fault_tolerance/test_recovery.cpp`
- **Report**: `/home/bbrelin/nimcp/RECOVERY_SYSTEM_DELIVERY_REPORT.md`

---

**Version**: 1.0.0
**Last Updated**: 2025-11-19
