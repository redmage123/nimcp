# Portia Learning Modes - Quick Reference

## Core Concepts

### Learning Modes (Bitmask)
```c
LEARNING_MODE_DISABLED      = 0      // No learning
LEARNING_MODE_HABITUATION   = 1 << 0 // Decrease response to repeated stimuli
LEARNING_MODE_SENSITIZATION = 1 << 1 // Increase response to important stimuli
LEARNING_MODE_ASSOCIATIVE   = 1 << 2 // Classical conditioning
LEARNING_MODE_TRIAL_ERROR   = 1 << 3 // Operant conditioning
LEARNING_MODE_OBSERVATIONAL = 1 << 4 // Learn from others (future)
LEARNING_MODE_FULL          = 0xFF   // All modes active
```

## Quick Start

```c
#include "portia/nimcp_portia_learning.h"

// 1. Configure
portia_learning_config_t config = {
    .allowed_modes = LEARNING_MODE_FULL,
    .max_habituation_entries = 64,
    .max_association_entries = 128,
    .default_learning_rate = 0.1f,
    .default_forgetting_rate = 0.01f,
    .consolidation_interval_ms = 60000,
    .habituation_threshold = 0.1f,
    .association_threshold = 0.05f
};

// 2. Initialize
portia_learning_state_t* learning = portia_learning_init(&config);

// 3. Use (see examples below)

// 4. Cleanup
portia_learning_destroy(learning);
```

## Common Operations

### Habituation (Ignore Repeated Stimuli)
```c
// Expose to stimulus repeatedly
uint64_t timestamp = nimcp_time_now();
portia_learning_habituate(learning, STIMULUS_ID, timestamp);

// Query response strength (decreases with exposures)
portia_learning_query_result_t result = portia_learning_query(learning, STIMULUS_ID);
if (result.found) {
    float strength = result.strength;  // 0.0-1.0 (lower = more habituated)
    uint32_t count = result.exposure_count;
}
```

### Sensitization (Heighten Response)
```c
// Boost response to important stimulus
float boost = 0.5f;  // 0.0-2.0
portia_learning_sensitize(learning, STIMULUS_ID, boost, nimcp_time_now());

// Query (strength > 1.0 indicates sensitization)
portia_learning_query_result_t result = portia_learning_query(learning, STIMULUS_ID);
```

### Associative Learning (Pavlov's Dog)
```c
// Create stimulus->response association
portia_learning_associate(learning, STIMULUS_ID, RESPONSE_ID,
                         true,  // positive association
                         nimcp_time_now());

// Query association strength
portia_learning_query_result_t result =
    portia_learning_query_association(learning, STIMULUS_ID, RESPONSE_ID);
if (result.found) {
    float strength = result.strength;  // 0.0-1.0
}
```

### Reinforcement Learning (Trial & Error)
```c
// Reward good outcomes, punish bad ones
float reward = 1.0f;    // positive reward
float punishment = -0.5f;  // negative reward

portia_learning_reinforce(learning, STIMULUS_ID, RESPONSE_ID, reward, nimcp_time_now());
```

### Maintenance
```c
// Apply forgetting (periodic)
portia_learning_forget(learning, nimcp_time_now());

// Consolidate memories (periodic)
portia_learning_consolidate(learning, nimcp_time_now());
```

## Configuration Guide

### Small/Embedded Systems
```c
portia_learning_config_t config = {
    .max_habituation_entries = 16,   // ~768 bytes
    .max_association_entries = 32,    // ~896 bytes
    .default_learning_rate = 0.2f,    // Faster learning
    .default_forgetting_rate = 0.05f, // Faster forgetting
    .consolidation_interval_ms = 30000 // Consolidate every 30s
};
// Total: ~1.7KB
```

### Standard Systems
```c
portia_learning_config_t config = {
    .max_habituation_entries = 64,    // ~3KB
    .max_association_entries = 128,   // ~3.5KB
    .default_learning_rate = 0.1f,
    .default_forgetting_rate = 0.01f,
    .consolidation_interval_ms = 60000 // Consolidate every minute
};
// Total: ~6.5KB
```

### High-Capacity Systems
```c
portia_learning_config_t config = {
    .max_habituation_entries = 1024,  // ~50KB
    .max_association_entries = 2048,  // ~57KB
    .default_learning_rate = 0.05f,   // Slower, more stable
    .default_forgetting_rate = 0.001f,// Very slow forgetting
    .consolidation_interval_ms = 300000 // Consolidate every 5 min
};
// Total: ~107KB
```

## Statistics & Monitoring

```c
portia_learning_stats_t stats = portia_learning_get_stats(learning);

printf("Habituation: %u/%u active (%.1f%% full)\n",
       stats.active_habituation_entries,
       stats.total_habituation_entries,
       100.0f * stats.active_habituation_entries / stats.total_habituation_entries);

printf("Associations: %u/%u active (%.1f%% full)\n",
       stats.active_association_entries,
       stats.total_association_entries,
       100.0f * stats.active_association_entries / stats.total_association_entries);

printf("Learning activity:\n");
printf("  Total exposures: %llu\n", stats.total_exposures);
printf("  Total reinforcements: %llu\n", stats.total_reinforcements);
printf("  Avg habituation strength: %.3f\n", stats.avg_habituation_strength);
printf("  Avg association strength: %.3f\n", stats.avg_association_strength);
```

## Mode Switching

```c
// Enable only specific modes
portia_learning_set_mode(learning,
    LEARNING_MODE_HABITUATION | LEARNING_MODE_ASSOCIATIVE);

// Disable all learning
portia_learning_set_mode(learning, LEARNING_MODE_DISABLED);

// Re-enable all
portia_learning_set_mode(learning, LEARNING_MODE_FULL);
```

## Export & Debug

```c
// Export current state to file for inspection
portia_learning_export(learning, "/tmp/learning_state.txt");

// Reset all learning (start fresh)
portia_learning_reset(learning);
```

## Performance Tips

### 1. Table Sizing
- **Too Small:** Frequent LRU evictions (lost learning)
- **Too Large:** Wasted memory, slower operations
- **Guideline:** Size for 2x expected active entries

### 2. Learning Rates
- **Fast Learning (0.2-0.5):** Quick adaptation, less stable
- **Medium (0.05-0.15):** Balanced
- **Slow (0.01-0.05):** Stable, requires more exposures

### 3. Forgetting Rates
- **Fast (0.05-0.1):** Short-term memory, adapt to changes
- **Medium (0.01-0.05):** Balanced retention
- **Slow (0.001-0.01):** Long-term memory, resist change

### 4. Consolidation Frequency
- **Frequent (10-30s):** Low memory, fast cleanup
- **Normal (1-5min):** Balanced
- **Infrequent (10-60min):** Allow weak entries to strengthen

## Common Patterns

### Pattern 1: Noise Filtering
```c
// Habituate to background noise
for (uint32_t i = 0; i < num_samples; i++) {
    if (is_background_noise(samples[i])) {
        portia_learning_habituate(learning, samples[i].id, timestamp);
    }
}

// Strong responses only to novel stimuli
portia_learning_query_result_t result = portia_learning_query(learning, stimulus_id);
if (!result.found || result.strength > 0.5f) {
    process_stimulus(stimulus_id);  // Only process novel/strong
}
```

### Pattern 2: Predictive Caching
```c
// Learn which items are frequently accessed together
if (accessed_item_A) {
    portia_learning_associate(learning, ITEM_A, ITEM_B, true, timestamp);
}

// Prefetch based on predictions
portia_learning_query_result_t result =
    portia_learning_query_association(learning, ITEM_A, ITEM_B);
if (result.found && result.strength > 0.7f) {
    prefetch_item(ITEM_B);  // High confidence prediction
}
```

### Pattern 3: Adaptive Behavior
```c
// Try different strategies, learn what works
for (uint32_t strategy_id = 0; strategy_id < NUM_STRATEGIES; strategy_id++) {
    bool success = try_strategy(strategy_id);
    float reward = success ? 1.0f : -0.5f;
    portia_learning_reinforce(learning, TASK_ID, strategy_id, reward, timestamp);
}

// Select best learned strategy
float best_strength = 0.0f;
uint32_t best_strategy = 0;
for (uint32_t strategy_id = 0; strategy_id < NUM_STRATEGIES; strategy_id++) {
    portia_learning_query_result_t result =
        portia_learning_query_association(learning, TASK_ID, strategy_id);
    if (result.found && result.strength > best_strength) {
        best_strength = result.strength;
        best_strategy = strategy_id;
    }
}
use_strategy(best_strategy);
```

### Pattern 4: Anomaly Detection
```c
// Learn normal patterns
portia_learning_habituate(learning, event_type, timestamp);

// Detect anomalies
portia_learning_query_result_t result = portia_learning_query(learning, event_type);
if (!result.found) {
    report_anomaly("Novel event type");
} else if (result.strength > 0.9f) {
    report_anomaly("Unusual frequency");
}
```

## Tuning Guide

### Problem: Learning too slow
- ✅ Increase `default_learning_rate`
- ✅ Decrease `habituation_rate` (modify code constant)
- ✅ Increase table capacities

### Problem: Forgetting too fast
- ✅ Decrease `default_forgetting_rate`
- ✅ Increase `consolidation_interval_ms`
- ✅ Lower association/habituation thresholds

### Problem: Memory full (evictions)
- ✅ Increase table capacities
- ✅ Increase forgetting rate (prune faster)
- ✅ Decrease consolidation interval (cleanup faster)
- ✅ Increase thresholds (higher bar for retention)

### Problem: Responding to habituated stimuli
- ✅ Increase `habituation_threshold` (require lower strength)
- ✅ Enable spontaneous recovery (wait 5+ minutes)
- ✅ Use sensitization to override habituation

## Error Handling

```c
portia_learning_state_t* learning = portia_learning_init(&config);
if (!learning) {
    LOG_ERROR("Failed to initialize learning system");
    return -1;
}

int result = portia_learning_habituate(learning, stimulus_id, timestamp);
if (result != 0) {
    LOG_ERROR("Habituation failed: %d", result);
}

// Always check query results
portia_learning_query_result_t query = portia_learning_query(learning, stimulus_id);
if (!query.found) {
    // Stimulus not yet learned
    use_default_response();
} else {
    // Use learned response
    scale_response_by(query.strength);
}
```

## Thread Safety

```c
// Multiple threads can safely call learning functions
// Mutex automatically protects internal state

// Thread 1
portia_learning_habituate(learning, STIMULUS_A, timestamp);

// Thread 2 (concurrent, safe)
portia_learning_associate(learning, STIMULUS_B, RESPONSE_B, true, timestamp);

// Thread 3 (concurrent, safe)
portia_learning_query_result_t result = portia_learning_query(learning, STIMULUS_A);
```

## Testing

```bash
# Build tests
cd build
cmake --build . --target test_portia_learning

# Run tests
./test/unit/portia/test_portia_learning

# Or use CTest
ctest -R PortiaLearning -V

# Run with labels
ctest -L portia -V
```

## Files Reference

| File | Path | Purpose |
|------|------|---------|
| Header | `/include/portia/nimcp_portia_learning.h` | API definitions |
| Implementation | `/src/portia/nimcp_portia_learning.c` | Core logic |
| Tests | `/test/unit/portia/test_portia_learning.cpp` | Unit tests |
| Build | `/src/portia/CMakeLists.txt` | Library build |
| Test Build | `/test/unit/portia/CMakeLists.txt` | Test build |

## Further Reading

- **Implementation Summary:** `docs/PORTIA_LEARNING_IMPLEMENTATION_SUMMARY.md`
- **Portia Main Docs:** `docs/PORTIA_SYSTEM_ARCHITECTURE.md` (if exists)
- **BBB Security:** `docs/SECURITY_VALIDATION_QUICK_START.md` (if exists)
- **Bio-Async:** `docs/BIO_ASYNC_INTEGRATION_COMPLETE_SUMMARY.md`

## Support

For issues or questions:
1. Check `LOG_ERROR` output for detailed error messages
2. Use `portia_learning_export()` to inspect state
3. Review `portia_learning_get_stats()` for diagnostics
4. Consult implementation summary for algorithm details
