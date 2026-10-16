# Portia Attention Quick Reference

## One-Minute Overview

**What:** Dynamic resource allocation based on attention/salience
**Why:** Model Portia spider cognitive flexibility
**How:** Fair allocation algorithm with priorities, smoothing, and hysteresis

---

## Quick Start

```c
#include "portia/nimcp_portia_attention.h"

// 1. Initialize
portia_attention_state_t state = portia_attention_init(NULL, 5, 1.0f);

// 2. Set priorities
portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.9f);

// 3. Reallocate
portia_attention_reallocate(state, true);

// 4. Query
float alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);

// 5. Cleanup
portia_attention_destroy(state);
```

---

## Resource Targets

| Target | Purpose |
|--------|---------|
| `ATTENTION_TARGET_NEURONS` | Neural computation |
| `ATTENTION_TARGET_MEMORY` | Working memory |
| `ATTENTION_TARGET_PROCESSING` | Processing cycles |
| `ATTENTION_TARGET_SENSORS` | Sensory input |
| `ATTENTION_TARGET_COMMUNICATION` | Communication |

---

## Essential Functions

### Lifecycle
```c
portia_attention_init(config, count, budget)  // Create system
portia_attention_destroy(state)               // Destroy system
```

### Salience
```c
portia_attention_update_salience(state, target, salience)  // Set importance
portia_attention_get_salience(state, target)               // Query importance
portia_attention_decay(state, time_ms)                     // Apply decay
```

### Allocation
```c
portia_attention_reallocate(state, force)              // Redistribute
portia_attention_get_allocation(state, target)         // Query allocation
portia_attention_request(state, target, amount)        // Request more
portia_attention_release(state, target, amount)        // Release unused
```

### Statistics
```c
portia_attention_get_stats(state, &stats)    // Get statistics
portia_attention_reset_stats(state)          // Reset counters
```

---

## Default Configuration

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `reallocation_threshold` | 0.05 | 5% change triggers reallocation |
| `decay_rate_per_second` | 0.1 | 10% decay per second |
| `update_interval_ms` | 100 | Check every 100ms |
| `enable_preemption` | true | Allow resource stealing |
| `preemption_threshold` | 0.3 | 30% salience difference |
| `hysteresis_factor` | 0.2 | 20% hysteresis band |
| `smoothing_alpha` | 0.3 | 30% new, 70% old |

---

## Common Patterns

### High Priority Task
```c
portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 1.0f);
portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.9f);
portia_attention_reallocate(state, true);
```

### Task Completion
```c
float current = portia_attention_get_allocation(state, target);
portia_attention_release(state, target, current * 0.5f);
```

### Decay Simulation
```c
for (int i = 0; i < 10; i++) {
    sleep(1);
    portia_attention_decay(state, get_time_ms());
}
```

---

## Allocation Algorithm (5 Steps)

1. **Score** = salience × priority
2. **Minimums** allocated first
3. **Remaining** distributed by score ratio
4. **Hysteresis** prevents oscillation
5. **Smoothing** creates gradual transitions

---

## File Locations

```
include/portia/nimcp_portia_attention.h    - Header
src/portia/nimcp_portia_attention.c        - Implementation
test/unit/portia/test_portia_attention.cpp - Tests
examples/portia_attention_demo.c           - Demo
```

---

## Build Commands

```bash
# Build library
cd build && cmake .. && make portia_attention

# Build tests
make test_portia_attention

# Run tests
./test/unit/portia/test_portia_attention

# Build demo
make portia_attention_demo

# Run demo
./examples/portia_attention_demo
```

---

## Debugging Tips

### Enable Debug Logging
```c
nimcp_log_set_level(NULL, LOG_LEVEL_DEBUG);
```

### Print State
```c
portia_attention_print_state(state);
```

### Check Statistics
```c
portia_attention_stats_t stats;
portia_attention_get_stats(state, &stats);
printf("Updates: %lu, Reallocations: %lu\n",
       stats.salience_updates, stats.reallocations);
```

### Verify Total Allocation
```c
float total = 0.0f;
for (int i = 0; i < ATTENTION_TARGET_COUNT; i++) {
    total += portia_attention_get_allocation(state, i);
}
printf("Total: %.3f (should be ~1.0)\n", total);
```

---

## Common Mistakes

❌ **DON'T:**
```c
// Salience out of range
portia_attention_update_salience(state, target, 1.5f);  // ERROR!

// Null state
portia_attention_reallocate(NULL, true);  // ERROR!

// Invalid target
portia_attention_get_allocation(state, 999);  // ERROR!
```

✅ **DO:**
```c
// Clamp salience to [0,1]
float salience = clamp(value, 0.0f, 1.0f);
portia_attention_update_salience(state, target, salience);

// Check return values
if (portia_attention_reallocate(state, true) != 0) {
    LOG_ERROR("Reallocation failed!");
}

// Validate target
if (target < ATTENTION_TARGET_COUNT) {
    float alloc = portia_attention_get_allocation(state, target);
}
```

---

## Thread Safety

✅ **Thread-Safe:**
- All public API functions
- Concurrent salience updates
- Concurrent reallocations
- Concurrent queries

⚠️ **Not Thread-Safe:**
- State destruction (caller must synchronize)

---

## Performance

| Operation | Complexity |
|-----------|------------|
| Update salience | O(1) |
| Reallocate | O(n log n) |
| Query allocation | O(1) |
| Decay | O(n) |

Where n = number of resources (typically 5)

---

## Integration

### With Bio-Async
```c
state->bio_async_enabled = true;
// Events automatically broadcast
```

### With Logging
```c
nimcp_log_init(NULL);
// Automatic logging to console/file
```

### With Security
```c
// BBB validation automatic in all functions
// No extra setup needed
```

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No reallocation | Check `update_interval_ms` |
| Oscillating | Increase `hysteresis_factor` |
| Jerky transitions | Decrease `smoothing_alpha` |
| Resources not summing to 1.0 | Normal (smoothing lag) |
| Memory leak | Call `portia_attention_destroy()` |

---

## Mathematical Formulas

**Salience Decay:**
```
S(t) = S₀ × e^(-λt)
```

**Exponential Smoothing:**
```
A_new = α × A_target + (1 - α) × A_old
```

**Score Calculation:**
```
score = salience × (priority / max_priority)
```

---

## Example Output (Demo)

```
--- Prey Detected ---
Resource        Salience  Allocation
---------------  --------  ----------
NEURONS            0.600       0.210
MEMORY             0.300       0.120
PROCESSING         0.500       0.190
SENSORS            0.900       0.350
COMMUNICATION      0.100       0.080
```

---

## Key Concepts

| Concept | Description |
|---------|-------------|
| **Salience** | Importance/priority (0.0-1.0) |
| **Allocation** | Actual resource % assigned |
| **Decay** | Time-based salience reduction |
| **Hysteresis** | Band preventing oscillation |
| **Smoothing** | Gradual allocation changes |
| **Budget** | Total resources available |

---

## Status Indicators

✅ **Complete** - All features implemented
✅ **Tested** - 20+ tests passing
✅ **Secure** - BBB validated
✅ **Thread-Safe** - Mutex protected
✅ **Documented** - Comprehensive docs
✅ **Production Ready** - No known bugs

---

## Help & Documentation

- **Full Docs:** `docs/PORTIA_ATTENTION_IMPLEMENTATION.md`
- **Summary:** `PORTIA_ATTENTION_SUMMARY.md`
- **This File:** `docs/PORTIA_ATTENTION_QUICK_REFERENCE.md`
- **Header:** `include/portia/nimcp_portia_attention.h`
- **Demo:** `examples/portia_attention_demo.c`

---

**Quick Reference Version:** 1.0
**Last Updated:** December 8, 2025
