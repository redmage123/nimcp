# NIMCP Metrics Implementation

**Version:** 2.6.1
**Date:** 2025-11-04
**Status:** Implemented

## Overview

The NIMCP Metrics system provides comprehensive metrics collection, export, and analysis capabilities with first-class support for:
- **Tableau** (CSV export format)
- **Microsoft PowerBI** (JSON export format)
- Real-time streaming to configurable directory
- Time-series data with timestamps
- Hierarchical brain performance tracking

## Architecture

### Components

1. **Core Metrics Module** (`nimcp_metrics.h`/`nimcp_metrics.c`)
   - Metrics collection and buffering
   - Time-series data storage
   - Export to multiple formats
   - Query API for analysis

2. **Python Bindings** (`nimcp_metrics_py.c`)
   - Complete Python API wrapper
   - Pythonic interface
   - Integration with nimcp Python module

3. **Ruby Bindings** (To be completed)
   - FFI-based wrapper
   - Idiomatic Ruby interface

4. **Node.js Bindings** (To be completed)
   - N-API wrapper
   - Promise-based async API

## Features

### Metric Types
- **Counter**: Monotonically increasing values
- **Gauge**: Point-in-time measurements
- **Histogram**: Distribution of values
- **Timer**: Duration measurements
- **Event**: Discrete events with labels

### Metric Categories
- **Performance**: Execution time, throughput
- **Memory**: Allocation, usage patterns
- **Network**: Communication metrics
- **Learning**: Training statistics
- **Inference**: Prediction metrics
- **System**: General system metrics
- **Custom**: User-defined metrics

### Export Formats
- **CSV**: Tableau-compatible format
- **JSON**: PowerBI-compatible format
- **Parquet**: (Future) Columnar format
- **TDE**: (Future) Tableau Data Extract

## Usage

### Python Example

```python
import nimcp

# Create metrics collector
metrics = nimcp.MetricsCollector(
    directory="./my_metrics",
    format=nimcp.METRICS_FORMAT_CSV
)

# Record various metrics
metrics.record_counter("forward_passes", 1000, nimcp.METRIC_CATEGORY_PERFORMANCE)
metrics.record_gauge("learning_rate", 0.001, nimcp.METRIC_CATEGORY_LEARNING)
metrics.record_timer("forward_time_ms", 15.5, nimcp.METRIC_CATEGORY_PERFORMANCE)

# Use performance timer
start = metrics.timer_start("training_epoch")
# ... do training ...
metrics.timer_stop("training_epoch", start, nimcp.METRIC_CATEGORY_LEARNING)

# Export for Tableau
metrics.export_tableau_csv("metrics_tableau.csv")

# Export for PowerBI
metrics.export_powerbi_json("metrics_powerbi.json")

# Get statistics
stats = metrics.get_stats()
print(stats)

# Flush to disk
count = metrics.flush()
print(f"Flushed {count} metrics")
```

### C Example

```c
#include "utils/metrics/nimcp_metrics.h"

// Create collector
nimcp_metrics_collector_t collector = nimcp_metrics_create();

// Record metrics
nimcp_metrics_record_counter(collector, "operations", 100,
                              NIMCP_METRIC_CATEGORY_PERFORMANCE);

nimcp_metrics_record_gauge(collector, "cpu_usage", 45.2,
                            NIMCP_METRIC_CATEGORY_SYSTEM);

// Record hierarchical brain metrics
nimcp_hierarchical_metrics_t brain_metrics = {
    .timestamp_ms = get_timestamp_ms(),
    .num_regions = 5,
    .total_forward_passes = 1000,
    .avg_forward_time_ms = 12.5,
    .dopamine_level = 0.8
};
nimcp_metrics_record_hierarchical(collector, &brain_metrics);

// Export
nimcp_metrics_export_tableau_csv(collector, "output.csv");
nimcp_metrics_export_powerbi_json(collector, "output.json");

// Cleanup
nimcp_metrics_destroy(collector);
```

## Configuration

### Metrics Directory

By default, metrics are stored in `./nimcp_metrics/`. This can be configured:

```python
# Python
metrics = nimcp.MetricsCollector(directory="/var/log/nimcp_metrics")
metrics.set_directory("/new/path/metrics")
```

```c
// C
nimcp_metrics_config_t config;
nimcp_metrics_get_default_config(&config);
strncpy(config.output_directory, "/var/log/nimcp_metrics",
        NIMCP_METRICS_MAX_PATH);
collector = nimcp_metrics_create_with_config(&config);
```

### Streaming Options

```c
config.enable_streaming = true;      // Real-time file streaming
config.flush_interval_ms = 5000;     // Auto-flush every 5 seconds
config.buffer_size = 10000;          // Buffer size before flush
config.enable_compression = false;   // Compress output (future)
```

## Tableau Integration

The CSV export format is optimized for Tableau:

**CSV Structure:**
```
timestamp_ms,name,type,category,value,labels
1699984123456,forward_passes,counter,performance,1000,{}
1699984123457,learning_rate,gauge,learning,0.001,{}
1699984123458,training_epoch,timer,learning,245.6,{"epoch":"5"}
```

**Tableau Import Steps:**
1. Open Tableau
2. Connect to Data → Text File
3. Select exported CSV file
4. Tableau auto-detects schema
5. Create visualizations using timestamp_ms for time-series

## PowerBI Integration

The JSON export format is optimized for PowerBI:

**JSON Structure:**
```json
[
  {
    "timestamp": 1699984123456,
    "name": "forward_passes",
    "type": "counter",
    "category": "performance",
    "value": 1000,
    "labels": {}
  }
]
```

**PowerBI Import Steps:**
1. Open PowerBI Desktop
2. Get Data → JSON
3. Select exported JSON file
4. Transform data if needed
5. Load into Power Query Editor
6. Create visualizations

## API Reference

### Python API

| Method | Description |
|--------|-------------|
| `__init__(directory, format)` | Create metrics collector |
| `record_counter(name, value, category)` | Record counter metric |
| `record_gauge(name, value, category)` | Record gauge metric |
| `record_timer(name, duration_ms, category)` | Record timer metric |
| `record_event(name, labels, category)` | Record event metric |
| `timer_start(name)` | Start performance timer |
| `timer_stop(name, start_time, category)` | Stop and record timer |
| `flush()` | Flush buffered metrics to disk |
| `export_tableau_csv(filename)` | Export to Tableau CSV |
| `export_powerbi_json(filename)` | Export to PowerBI JSON |
| `get_stats()` | Get JSON statistics |
| `set_directory(path)` | Change metrics directory |

### C API

See `src/utils/metrics/nimcp_metrics.h` for complete C API documentation.

## File Organization

```
nimcp_metrics/
├── metrics_1699984123456.csv  # Streaming file (auto-created)
├── metrics_tableau.csv         # Tableau export
└── metrics_powerbi.json        # PowerBI export
```

## Performance Considerations

- **Buffering**: Metrics are buffered in memory (default: 10,000 entries)
- **Auto-flush**: Automatic flush to disk based on interval or buffer size
- **Streaming**: Real-time streaming for immediate visibility
- **Query Performance**: In-memory queries are O(n), optimized for recent data

## Future Enhancements

1. **Parquet Format**: Columnar storage for large datasets
2. **Tableau TDE**: Native Tableau Data Extract format
3. **Compression**: Gzip compression for CSV/JSON
4. **Aggregation**: Built-in aggregation functions (avg, sum, min, max)
5. **Time Windows**: Automatic time-based partitioning
6. **Retention Policies**: Automatic cleanup of old metrics
7. **Remote Export**: HTTP/HTTPS push to remote servers
8. **Grafana Integration**: Prometheus-compatible export

## Integration with Hierarchical Brain

The metrics system is designed to track hierarchical brain performance:

```python
# Collect hierarchical brain metrics
brain_metrics = {
    'num_regions': hierarchical_get_num_regions(hbrain),
    'num_layers': hierarchical_get_num_layers(hbrain),
    'total_forward_passes': hierarchical_get_total_forward_passes(hbrain),
    'dopamine': hierarchical_get_dopamine(hbrain),
    # ... more metrics
}

# Record to metrics system
metrics.record_gauge("brain.num_regions", brain_metrics['num_regions'],
                     nimcp.METRIC_CATEGORY_SYSTEM)
```

## Testing

Unit tests are available in `src/tests/test_metrics.cpp`:

```bash
# Build tests
cmake --build . --target utility_tests

# Run metrics tests
./src/tests/utility_tests --gtest_filter="Metrics*"
```

## Troubleshooting

### Directory Permission Issues
```
Error: Failed to create metrics directory
Solution: Ensure write permissions on parent directory
```

### File Size Growth
```
Issue: Metrics files growing too large
Solution:
1. Reduce flush_interval_ms
2. Decrease buffer_size
3. Implement retention policies
```

### Performance Impact
```
Issue: Metrics collection slowing down application
Solution:
1. Disable streaming (enable_streaming = false)
2. Increase buffer_size
3. Use separate thread for metrics (future feature)
```

## License

Part of NIMCP project. See main LICENSE file.

## Contributors

- NIMCP Development Team
- Claude Code (AI Assistant)

## References

- [Tableau Data Format Documentation](https://help.tableau.com/current/pro/desktop/en-us/examples_csv.htm)
- [PowerBI JSON Import](https://learn.microsoft.com/en-us/power-bi/connect-data/desktop-connect-json)
- NIMCP Core Documentation
