# Portia Sensor Fusion - Quick Reference

## Quick Start

```c
#include "portia/nimcp_portia_sensor_fusion.h"

// 1. Initialize
portia_fusion_config_t config = portia_fusion_default_config();
portia_fusion_ctx_t* fusion = portia_fusion_init(&config, NULL);

// 2. Update sensors
sensor_reading_t reading = {
    .type = SENSOR_TYPE_VISUAL,
    .value = 10.0f,
    .confidence = 0.9f,
    .timestamp_ms = nimcp_platform_get_time_ms(),
    .valid = true
};
portia_fusion_update_sensor(fusion, &reading);

// 3. Process
portia_fusion_process(fusion);

// 4. Get result
fused_state_t state;
portia_fusion_get_state(fusion, &state);

// 5. Cleanup
portia_fusion_destroy(fusion);
```

## Sensor Types

| Type | Best For | Update Rate | Typical Confidence |
|------|----------|-------------|-------------------|
| `SENSOR_TYPE_VISUAL` | Position, tracking | 30-60 Hz | 0.85-0.95 |
| `SENSOR_TYPE_AUDIO` | Sound localization | 40-100 Hz | 0.7-0.85 |
| `SENSOR_TYPE_VIBRATION` | Seismic detection | 10-50 Hz | 0.7-0.8 |
| `SENSOR_TYPE_CHEMICAL` | Odor tracking | 1-10 Hz | 0.5-0.7 |
| `SENSOR_TYPE_THERMAL` | Heat detection | 5-20 Hz | 0.75-0.85 |
| `SENSOR_TYPE_PROXIMITY` | Distance measurement | 10-50 Hz | 0.8-0.9 |
| `SENSOR_TYPE_IMU` | Motion tracking | 100-500 Hz | 0.85-0.95 |
| `SENSOR_TYPE_GPS` | Location | 1-10 Hz | 0.6-0.9 |

## Configuration Options

### Fusion Modes

```c
// Weighted Average (fast, lightweight)
config.enable_kalman = false;
config.fusion_rate_hz = 50;

// Kalman Filter (accurate, predictive)
config.enable_kalman = true;
config.process_noise = 0.05f;
config.fusion_rate_hz = 30;
```

### Sensor Configuration

```c
// Per-sensor settings
config.sensors[SENSOR_TYPE_VISUAL].weight = 0.8f;
config.sensors[SENSOR_TYPE_VISUAL].noise_variance = 0.1f;
config.sensors[SENSOR_TYPE_VISUAL].update_rate_hz = 50;
config.sensors[SENSOR_TYPE_VISUAL].enabled = true;
config.sensors[SENSOR_TYPE_VISUAL].required = false;
```

### Robustness Settings

```c
config.outlier_threshold = 3.0f;     // Sigma for outlier rejection
config.min_sensors = 2;               // Minimum active sensors
config.enable_fallback = true;        // Single-sensor fallback
```

## API Functions

### Initialization
```c
portia_fusion_ctx_t* portia_fusion_init(
    const portia_fusion_config_t* config,
    nimcp_bio_ctx_t* bio_ctx  // Optional, can be NULL
);

void portia_fusion_destroy(portia_fusion_ctx_t* ctx);

portia_fusion_config_t portia_fusion_default_config(void);
```

### Core Operations
```c
// Add sensor reading
bool portia_fusion_update_sensor(
    portia_fusion_ctx_t* ctx,
    const sensor_reading_t* reading
);

// Run fusion algorithm
bool portia_fusion_process(portia_fusion_ctx_t* ctx);

// Get fused state
bool portia_fusion_get_state(
    const portia_fusion_ctx_t* ctx,
    fused_state_t* state
);
```

### Configuration
```c
// Adjust sensor weight at runtime
bool portia_fusion_set_weight(
    portia_fusion_ctx_t* ctx,
    sensor_type_t type,
    float weight  // [0.0 - 1.0]
);

// Enable/disable sensor
bool portia_fusion_enable_sensor(
    portia_fusion_ctx_t* ctx,
    sensor_type_t type,
    bool enabled
);
```

### Monitoring
```c
// Get overall confidence
float portia_fusion_get_confidence(const portia_fusion_ctx_t* ctx);

// Get statistics
bool portia_fusion_get_stats(
    const portia_fusion_ctx_t* ctx,
    portia_fusion_stats_t* stats
);

// Reset state
bool portia_fusion_reset(portia_fusion_ctx_t* ctx);
```

## Data Structures

### Sensor Reading
```c
typedef struct {
    sensor_type_t type;
    float value;              // Primary measurement
    float confidence;         // [0.0 - 1.0]
    uint64_t timestamp_ms;
    bool valid;
} sensor_reading_t;
```

### Fused State
```c
typedef struct {
    float x, y, z;            // Position
    float vx, vy, vz;         // Velocity
    float heading;            // Direction (radians)
    float confidence;         // Overall confidence
    uint64_t timestamp_ms;
    uint32_t contributing_sensors;  // Bitmask
} fused_state_t;
```

### Statistics
```c
typedef struct {
    uint64_t total_updates;
    uint64_t successful_fusions;
    uint64_t outliers_rejected;
    uint64_t sensor_dropouts;
    float average_confidence;
    uint32_t active_sensor_count;
} portia_fusion_stats_t;
```

## Common Patterns

### Pattern 1: High-Frequency Visual + IMU
```c
config.enable_kalman = true;
config.fusion_rate_hz = 100;
config.sensors[SENSOR_TYPE_VISUAL].weight = 0.6f;
config.sensors[SENSOR_TYPE_IMU].weight = 0.4f;
```

### Pattern 2: Multi-Modal Tracking
```c
config.enable_kalman = false;  // Fast
config.min_sensors = 3;
// Enable visual, audio, vibration
for (int i = 0; i < 3; i++) {
    config.sensors[i].enabled = true;
    config.sensors[i].weight = 0.33f;
}
```

### Pattern 3: Sparse Sensor Fusion
```c
config.enable_fallback = true;
config.min_sensors = 1;
config.outlier_threshold = 4.0f;  // More tolerant
```

### Pattern 4: Reliable Sensors Only
```c
config.min_sensors = 2;
config.sensors[SENSOR_TYPE_VISUAL].required = true;
config.sensors[SENSOR_TYPE_IMU].required = true;
```

## Performance Tips

1. **Use weighted average for high-frequency**: < 1ms per fusion
2. **Use Kalman for tracking**: Better state estimation
3. **Disable unused sensors**: Reduces processing overhead
4. **Adjust weights dynamically**: Based on sensor quality
5. **Set appropriate outlier threshold**: Balance rejection vs. data loss
6. **Monitor statistics**: Track outliers and dropouts

## Error Handling

All functions return `bool` (success/failure) or `NULL` on error:

```c
if (!portia_fusion_update_sensor(fusion, &reading)) {
    // Check logs for specific error
    LOG_ERROR("Sensor update failed");
}

if (!portia_fusion_process(fusion)) {
    // Possible reasons:
    // - Insufficient active sensors
    // - All data is stale
    // - No valid readings
}
```

## Thread Safety

All runtime operations are thread-safe:
- ✅ `portia_fusion_update_sensor()`
- ✅ `portia_fusion_process()`
- ✅ `portia_fusion_get_state()`
- ✅ `portia_fusion_set_weight()`
- ✅ `portia_fusion_enable_sensor()`

Not thread-safe (single-threaded use only):
- ❌ `portia_fusion_init()`
- ❌ `portia_fusion_destroy()`

## Bio-Async Events

When bio-async context is provided, events are broadcast:

```c
// Event format
"fusion_event:type={event_type},confidence={conf},sensors={mask},timestamp={ts}"

// Event types
"init"           - System initialized
"fusion_update"  - Fusion processed
"reset"          - State reset
"destroy"        - System shutting down
```

## Testing

```bash
# Build
cd build
cmake --build . --target test_portia_sensor_fusion

# Run unit tests
./test/unit/portia/test_portia_sensor_fusion

# Run integration tests
./test/integration/portia/test_portia_sensor_fusion_integration

# Run demo
./examples/portia_sensor_fusion_demo
```

## Debugging

Enable debug logging:
```c
// In your code before init
nimcp_log_set_level(LOG_LEVEL_DEBUG);

// Now you'll see detailed fusion logs
```

Check statistics:
```c
portia_fusion_stats_t stats;
portia_fusion_get_stats(fusion, &stats);
printf("Outliers: %lu\n", stats.outliers_rejected);
printf("Avg confidence: %.3f\n", stats.average_confidence);
```

## Security Notes

- All pointers validated with BBB
- Range checking on all parameters
- Security audit logging on critical events
- No buffer overflows or memory leaks
- Magic number context validation

## Code Standards

✅ **Correct Memory Functions**:
- `nimcp_malloc()`, `nimcp_calloc()`, `nimcp_free()`

✅ **Correct Logging**:
- `LOG_DEBUG()`, `LOG_INFO()`, `LOG_WARN()`, `LOG_ERROR()`

✅ **Required Validation**:
- `bbb_validate_pointer()` for all pointers
- `bbb_validate_range()` for numerical ranges
- `bbb_audit_log()` for security events

## Files

- **Header**: `include/portia/nimcp_portia_sensor_fusion.h`
- **Implementation**: `src/portia/nimcp_portia_sensor_fusion.c`
- **Unit Tests**: `test/unit/portia/test_portia_sensor_fusion.cpp`
- **Integration Tests**: `test/integration/portia/test_portia_sensor_fusion_integration.cpp`
- **Demo**: `examples/portia_sensor_fusion_demo.c`
- **Full Docs**: `docs/PORTIA_SENSOR_FUSION_IMPLEMENTATION.md`
