# Brain Probe API

**Version:** 2.6.1
**Date:** 2025-11-04
**Status:** Implemented

## Overview

The Brain Probe API provides a comprehensive snapshot of a brain's current state, including architecture, performance metrics, and resource usage. This is similar to network probing functions and enables monitoring, debugging, and analysis of brain instances.

## API Reference

### Data Structure

```c
typedef struct {
    char task_name[64];           // Brain name
    nimcp_brain_size_t size;      // Size preset (TINY/SMALL/MEDIUM/LARGE)
    nimcp_brain_task_t task;      // Task type
    uint32_t num_neurons;         // Total neurons
    uint32_t num_synapses;        // Total synapses
    uint32_t num_active_synapses; // Non-pruned synapses

    uint64_t total_inferences;     // Total inference count
    uint64_t total_learning_steps; // Total learning steps

    float avg_sparsity;          // Average sparsity (0.0-1.0)
    float avg_inference_time_us; // Average inference time (microseconds)
    float current_learning_rate; // Current learning rate

    float accuracy;      // Validation accuracy (0.0-1.0)
    size_t memory_bytes; // Memory usage in bytes

    uint32_t num_inputs;  // Number of inputs
    uint32_t num_outputs; // Number of outputs
} nimcp_brain_probe_t;
```

### Function

```c
nimcp_status_t nimcp_brain_probe(nimcp_brain_t brain, nimcp_brain_probe_t* probe);
```

**Parameters:**
- `brain`: Brain handle to probe
- `probe`: Output structure to fill with statistics

**Returns:**
- `NIMCP_OK` on success
- `NIMCP_ERROR_NULL_ARG` if brain or probe is NULL
- `NIMCP_ERROR` if probing fails

## Usage Examples

### C Example

```c
#include "include/nimcp.h"
#include <stdio.h>

int main(void) {
    // Initialize
    nimcp_init();

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "my_brain",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        10, 3
    );

    // Probe brain state
    nimcp_brain_probe_t probe;
    if (nimcp_brain_probe(brain, &probe) == NIMCP_OK) {
        printf("Brain: %s\n", probe.task_name);
        printf("Neurons: %u\n", probe.num_neurons);
        printf("Synapses: %u (active: %u)\n",
               probe.num_synapses, probe.num_active_synapses);
        printf("Inferences: %lu\n", probe.total_inferences);
        printf("Learning steps: %lu\n", probe.total_learning_steps);
        printf("Memory: %.2f MB\n", probe.memory_bytes / (1024.0 * 1024.0));
    }

    // Cleanup
    nimcp_brain_destroy(brain);
    nimcp_shutdown();
    return 0;
}
```

### Python Example (Future)

```python
import nimcp

# Create brain
brain = nimcp.Brain(
    name="my_brain",
    size=nimcp.BRAIN_SMALL,
    task=nimcp.TASK_CLASSIFICATION,
    num_inputs=10,
    num_outputs=3
)

# Probe brain
probe = brain.probe()
print(f"Neurons: {probe.num_neurons}")
print(f"Synapses: {probe.num_synapses} (active: {probe.num_active_synapses})")
print(f"Inferences: {probe.total_inferences}")
print(f"Memory: {probe.memory_bytes / 1024 / 1024:.2f} MB")
```

### Ruby Example (Future)

```ruby
require 'nimcp'

# Create brain
brain = NIMCP::Brain.new(
  name: "my_brain",
  size: :small,
  task: :classification,
  num_inputs: 10,
  num_outputs: 3
)

# Probe brain
probe = brain.probe
puts "Neurons: #{probe.num_neurons}"
puts "Synapses: #{probe.num_synapses} (active: #{probe.num_active_synapses})"
puts "Inferences: #{probe.total_inferences}"
puts "Memory: #{probe.memory_bytes / 1024.0 / 1024.0} MB"
```

### Node.js Example (Future)

```javascript
const nimcp = require('nimcp');

// Create brain
const brain = new nimcp.Brain({
    name: "my_brain",
    size: nimcp.BRAIN_SMALL,
    task: nimcp.TASK_CLASSIFICATION,
    num_inputs: 10,
    num_outputs: 3
});

// Probe brain
const probe = brain.probe();
console.log(`Neurons: ${probe.num_neurons}`);
console.log(`Synapses: ${probe.num_synapses} (active: ${probe.num_active_synapses})`);
console.log(`Inferences: ${probe.total_inferences}`);
console.log(`Memory: ${probe.memory_bytes / 1024 / 1024} MB`);
```

## Use Cases

1. **Monitoring**: Track brain performance metrics over time
2. **Debugging**: Diagnose issues with learning or inference
3. **Resource Management**: Monitor memory usage and optimize
4. **Performance Tuning**: Analyze inference times and optimize
5. **System Health**: Check brain state in production systems
6. **Telemetry**: Export metrics to monitoring systems

## Metrics Collected

### Architecture Metrics
- **Neurons**: Total number of neurons in the brain
- **Synapses**: Total connections between neurons
- **Active Synapses**: Non-pruned connections (after learning)
- **Inputs/Outputs**: Dimension of input and output spaces

### Performance Metrics
- **Total Inferences**: Cumulative count of predictions made
- **Total Learning Steps**: Cumulative count of training iterations
- **Avg Inference Time**: Average time per inference (microseconds)

### Learning Metrics
- **Learning Rate**: Current learning rate value
- **Sparsity**: Percentage of inactive neurons (0.0-1.0)
- **Accuracy**: Validation accuracy if available (0.0-1.0)

### Resource Metrics
- **Memory Usage**: Total memory allocated by brain (bytes)

## Integration with Metrics System

The brain probe can be integrated with the metrics collection system:

```c
#include "include/nimcp.h"
#include "utils/metrics/nimcp_metrics.h"

// Create metrics collector
nimcp_metrics_collector_t metrics = nimcp_metrics_create();

// Probe brain
nimcp_brain_probe_t probe;
nimcp_brain_probe(brain, &probe);

// Record metrics
nimcp_metrics_record_gauge(metrics, "brain.neurons", probe.num_neurons,
                           NIMCP_METRIC_CATEGORY_SYSTEM);
nimcp_metrics_record_gauge(metrics, "brain.synapses", probe.num_synapses,
                           NIMCP_METRIC_CATEGORY_SYSTEM);
nimcp_metrics_record_counter(metrics, "brain.inferences", probe.total_inferences,
                             NIMCP_METRIC_CATEGORY_PERFORMANCE);
nimcp_metrics_record_gauge(metrics, "brain.memory_mb",
                           probe.memory_bytes / (1024.0 * 1024.0),
                           NIMCP_METRIC_CATEGORY_MEMORY);

// Export to Tableau/PowerBI
nimcp_metrics_export_tableau_csv(metrics, "brain_metrics.csv");
nimcp_metrics_export_powerbi_json(metrics, "brain_metrics.json");
```

## Demo Program

A comprehensive example is available at:
```
examples/brain_probe_demo.c
```

Build and run:
```bash
make brain_probe_demo
./examples/brain_probe_demo
```

## See Also

- [METRICS_IMPLEMENTATION.md](METRICS_IMPLEMENTATION.md) - Metrics collection system
- [METRICS_CATALOG.md](METRICS_CATALOG.md) - Complete metrics catalog
- [nimcp.h](../src/include/nimcp.h) - Public API reference

## License

Part of NIMCP project. See main LICENSE file.
