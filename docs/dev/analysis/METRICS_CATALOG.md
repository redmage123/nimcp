# NIMCP Metrics Catalog

**Version:** 2.6.1
**Date:** 2025-11-04

## Overview

This document provides a comprehensive catalog of all standard metrics available in NIMCP. Use these standardized metric names to ensure consistency across dashboards and analytics.

## Naming Convention

Metrics follow this pattern: `<module>.<component>.<metric_name>`

Examples:
- `hierarchical.forward_passes` - Hierarchical brain forward passes
- `brain.learning.error` - Brain learning error rate
- `system.memory.allocated_bytes` - System memory allocation

## Metric Categories

- 🚀 **Performance**: Execution time, throughput, latency
- 💾 **Memory**: Allocation, usage, garbage collection
- 🌐 **Network**: Communication, bandwidth, latency
- 🧠 **Learning**: Training metrics, accuracy, loss
- 🔮 **Inference**: Prediction metrics, confidence
- ⚙️ **System**: General system health
- 🎯 **Custom**: User-defined metrics

---

## 1. Hierarchical Brain Metrics

### System Metrics (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `hierarchical.num_regions` | Gauge | Number of brain regions | count |
| `hierarchical.num_layers` | Gauge | Number of hierarchical layers | count |
| `hierarchical.active_regions` | Gauge | Currently active regions | count |
| `hierarchical.saturated_regions` | Gauge | Saturated/overloaded regions | count |

### Performance Metrics (Category: PERFORMANCE)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `hierarchical.forward_passes` | Counter | Total forward passes executed | count |
| `hierarchical.avg_forward_time_ms` | Gauge | Average forward pass duration | milliseconds |
| `hierarchical.forward_pass_latency_p50` | Gauge | 50th percentile forward latency | milliseconds |
| `hierarchical.forward_pass_latency_p95` | Gauge | 95th percentile forward latency | milliseconds |
| `hierarchical.forward_pass_latency_p99` | Gauge | 99th percentile forward latency | milliseconds |
| `hierarchical.throughput_fps` | Gauge | Forward passes per second | fps |

### Learning Metrics (Category: LEARNING)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `hierarchical.learning_updates` | Counter | Total learning updates | count |
| `hierarchical.avg_learning_time_ms` | Gauge | Average learning duration | milliseconds |
| `hierarchical.avg_learning_rate` | Gauge | Current learning rate | float |
| `hierarchical.avg_error` | Gauge | Average prediction error | float |
| `hierarchical.avg_accuracy` | Gauge | Average accuracy | percentage |
| `hierarchical.training_epochs` | Counter | Training epochs completed | count |
| `hierarchical.batch_size` | Gauge | Current batch size | count |
| `hierarchical.gradient_norm` | Gauge | Gradient norm magnitude | float |

### Memory Metrics (Category: MEMORY)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `hierarchical.total_memory_bytes` | Gauge | Total memory allocated | bytes |
| `hierarchical.active_memory_bytes` | Gauge | Currently active memory | bytes |
| `hierarchical.num_allocations` | Counter | Total memory allocations | count |
| `hierarchical.memory_utilization` | Gauge | Memory utilization percentage | percentage |
| `hierarchical.peak_memory_bytes` | Gauge | Peak memory usage | bytes |

### Neuromodulation Metrics (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `hierarchical.dopamine` | Gauge | Dopamine level | 0.0-1.0 |
| `hierarchical.acetylcholine` | Gauge | Acetylcholine level | 0.0-1.0 |
| `hierarchical.serotonin` | Gauge | Serotonin level | 0.0-1.0 |

---

## 2. Region-Level Metrics

### Per-Region Performance (Category: PERFORMANCE)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `region.<name>.activations` | Counter | Total activations for region | count |
| `region.<name>.activation_rate` | Gauge | Activations per second | hz |
| `region.<name>.avg_activation_time_ms` | Gauge | Average activation time | milliseconds |
| `region.<name>.processing_latency_ms` | Gauge | Processing latency | milliseconds |

### Per-Region Learning (Category: LEARNING)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `region.<name>.updates` | Counter | Total learning updates | count |
| `region.<name>.learning_rate` | Gauge | Current learning rate | float |
| `region.<name>.error` | Gauge | Current error | float |
| `region.<name>.weight_updates` | Counter | Weight update count | count |

### Per-Region Connections (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `region.<name>.num_inputs` | Gauge | Input connections | count |
| `region.<name>.num_feedback` | Gauge | Feedback connections | count |
| `region.<name>.num_lateral` | Gauge | Lateral connections | count |
| `region.<name>.connection_density` | Gauge | Connection density | percentage |

### Per-Region Memory (Category: MEMORY)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `region.<name>.memory_enabled` | Gauge | Working memory status | boolean |
| `region.<name>.memory_utilization` | Gauge | Memory buffer usage | percentage |
| `region.<name>.memory_decay_rate` | Gauge | Memory decay rate | float |

---

## 3. Core Brain Metrics

### Neural Network Performance (Category: PERFORMANCE)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `brain.inference_time_ms` | Timer | Inference duration | milliseconds |
| `brain.forward_pass_time_ms` | Timer | Forward pass duration | milliseconds |
| `brain.backward_pass_time_ms` | Timer | Backward pass duration | milliseconds |
| `brain.batch_processing_time_ms` | Timer | Batch processing time | milliseconds |

### Neural Network Learning (Category: LEARNING)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `brain.learning.loss` | Gauge | Current loss value | float |
| `brain.learning.accuracy` | Gauge | Current accuracy | percentage |
| `brain.learning.f1_score` | Gauge | F1 score | 0.0-1.0 |
| `brain.learning.precision` | Gauge | Precision | percentage |
| `brain.learning.recall` | Gauge | Recall | percentage |
| `brain.learning.confusion_matrix` | Event | Confusion matrix snapshot | json |

### Neural Network Structure (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `brain.num_neurons` | Gauge | Total neuron count | count |
| `brain.num_synapses` | Gauge | Total synapse count | count |
| `brain.num_layers` | Gauge | Network depth | count |
| `brain.sparsity` | Gauge | Connection sparsity | percentage |

---

## 4. Plasticity Metrics

### BCM Plasticity (Category: LEARNING)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `plasticity.bcm.threshold` | Gauge | BCM threshold | float |
| `plasticity.bcm.tau` | Gauge | BCM time constant | milliseconds |
| `plasticity.bcm.updates` | Counter | BCM weight updates | count |

### STDP Plasticity (Category: LEARNING)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `plasticity.stdp.potentiation` | Counter | LTP events | count |
| `plasticity.stdp.depression` | Counter | LTD events | count |
| `plasticity.stdp.tau_plus` | Gauge | Positive time constant | milliseconds |
| `plasticity.stdp.tau_minus` | Gauge | Negative time constant | milliseconds |

### Attention Mechanism (Category: PERFORMANCE)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `attention.focus_regions` | Gauge | Regions under attention | count |
| `attention.avg_gain` | Gauge | Average attention gain | float |
| `attention.shifts` | Counter | Attention shift count | count |

---

## 5. Network/P2P Metrics

### Communication (Category: NETWORK)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `network.messages_sent` | Counter | Messages sent | count |
| `network.messages_received` | Counter | Messages received | count |
| `network.bytes_sent` | Counter | Bytes transmitted | bytes |
| `network.bytes_received` | Counter | Bytes received | bytes |
| `network.avg_latency_ms` | Gauge | Average network latency | milliseconds |
| `network.packet_loss_rate` | Gauge | Packet loss rate | percentage |

### P2P Node (Category: NETWORK)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `p2p.num_peers` | Gauge | Connected peers | count |
| `p2p.num_connections` | Gauge | Active connections | count |
| `p2p.connection_errors` | Counter | Connection failures | count |
| `p2p.replication_lag_ms` | Gauge | Replication lag | milliseconds |

### Distributed Cognition (Category: NETWORK)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `distributed.sync_time_ms` | Timer | Synchronization time | milliseconds |
| `distributed.consensus_rounds` | Counter | Consensus rounds | count |
| `distributed.knowledge_shared_bytes` | Counter | Knowledge transferred | bytes |

---

## 6. I/O and Serialization Metrics

### Data I/O (Category: PERFORMANCE)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `io.read_time_ms` | Timer | Read operation time | milliseconds |
| `io.write_time_ms` | Timer | Write operation time | milliseconds |
| `io.bytes_read` | Counter | Bytes read | bytes |
| `io.bytes_written` | Counter | Bytes written | bytes |
| `io.read_errors` | Counter | Read errors | count |
| `io.write_errors` | Counter | Write errors | count |

### Serialization (Category: PERFORMANCE)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `serialization.serialize_time_ms` | Timer | Serialization time | milliseconds |
| `serialization.deserialize_time_ms` | Timer | Deserialization time | milliseconds |
| `serialization.compressed_size_bytes` | Gauge | Compressed size | bytes |
| `serialization.compression_ratio` | Gauge | Compression ratio | float |

---

## 7. System-Level Metrics

### Memory Management (Category: MEMORY)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `system.memory.allocated_bytes` | Gauge | Total allocated memory | bytes |
| `system.memory.freed_bytes` | Counter | Memory freed | bytes |
| `system.memory.num_allocs` | Counter | Allocation count | count |
| `system.memory.num_frees` | Counter | Free count | count |
| `system.memory.fragmentation` | Gauge | Memory fragmentation | percentage |
| `system.memory.peak_usage_bytes` | Gauge | Peak memory usage | bytes |

### Thread Pool (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `threadpool.num_threads` | Gauge | Active threads | count |
| `threadpool.queue_depth` | Gauge | Task queue depth | count |
| `threadpool.tasks_completed` | Counter | Completed tasks | count |
| `threadpool.avg_task_time_ms` | Gauge | Average task duration | milliseconds |
| `threadpool.thread_utilization` | Gauge | Thread utilization | percentage |

### Process (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `system.cpu_usage` | Gauge | CPU usage | percentage |
| `system.resident_memory_mb` | Gauge | Resident memory | megabytes |
| `system.virtual_memory_mb` | Gauge | Virtual memory | megabytes |
| `system.open_file_descriptors` | Gauge | Open file descriptors | count |
| `system.uptime_seconds` | Counter | Process uptime | seconds |

---

## 8. Glial Cell Metrics

### Astrocytes (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `glial.astrocytes.num_active` | Gauge | Active astrocytes | count |
| `glial.astrocytes.calcium_level` | Gauge | Average calcium level | float |
| `glial.astrocytes.modulation_events` | Counter | Synaptic modulation events | count |

### Microglia (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `glial.microglia.num_active` | Gauge | Active microglia | count |
| `glial.microglia.pruning_events` | Counter | Synapse pruning events | count |
| `glial.microglia.surveillance_cycles` | Counter | Surveillance cycles | count |

### Oligodendrocytes (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `glial.oligo.num_active` | Gauge | Active oligodendrocytes | count |
| `glial.oligo.myelination_level` | Gauge | Average myelination | percentage |
| `glial.oligo.signal_speed_boost` | Gauge | Signal speed increase | float |

---

## 9. Cognitive Metrics

### Knowledge (Category: LEARNING)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `cognitive.knowledge.concepts_stored` | Gauge | Stored concepts | count |
| `cognitive.knowledge.retrieval_time_ms` | Timer | Concept retrieval time | milliseconds |
| `cognitive.knowledge.graph_size` | Gauge | Knowledge graph size | count |

### Curiosity (Category: LEARNING)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `cognitive.curiosity.exploration_rate` | Gauge | Exploration rate | percentage |
| `cognitive.curiosity.novelty_score` | Gauge | Novelty detection score | float |
| `cognitive.curiosity.exploration_events` | Counter | Exploration events | count |

### Ethics (Category: SYSTEM)

| Metric Name | Type | Description | Unit |
|-------------|------|-------------|------|
| `cognitive.ethics.evaluations` | Counter | Ethics evaluations | count |
| `cognitive.ethics.violations` | Counter | Ethics violations detected | count |
| `cognitive.ethics.avg_score` | Gauge | Average ethics score | float |

---

## 10. Custom Metrics Guidelines

### Creating Custom Metrics

When creating custom metrics, follow these guidelines:

1. **Use hierarchical naming**: `<module>.<component>.<metric>`
2. **Choose appropriate type**:
   - Counter: Always increasing (requests, errors)
   - Gauge: Point-in-time value (temperature, queue depth)
   - Timer: Duration measurements
   - Histogram: Distribution of values
   - Event: Discrete occurrences with metadata

3. **Add labels for dimensions**:
   ```python
   metrics.record_event("experiment.completed",
                        labels='{"model":"v2", "dataset":"mnist"}',
                        category=nimcp.METRIC_CATEGORY_CUSTOM)
   ```

4. **Document your metrics**: Add to this catalog

---

## API Usage Examples

### Python API

```python
import nimcp

metrics = nimcp.MetricsCollector()

# Hierarchical brain metrics
metrics.record_gauge("hierarchical.num_regions", 5,
                     nimcp.METRIC_CATEGORY_SYSTEM)
metrics.record_counter("hierarchical.forward_passes", 1000,
                       nimcp.METRIC_CATEGORY_PERFORMANCE)

# Performance timing
start = metrics.timer_start("hierarchical.forward_pass")
# ... do forward pass ...
metrics.timer_stop("hierarchical.forward_pass", start,
                   nimcp.METRIC_CATEGORY_PERFORMANCE)

# Learning metrics
metrics.record_gauge("hierarchical.avg_error", 0.023,
                     nimcp.METRIC_CATEGORY_LEARNING)
metrics.record_gauge("hierarchical.avg_accuracy", 0.977,
                     nimcp.METRIC_CATEGORY_LEARNING)

# Memory metrics
metrics.record_gauge("system.memory.allocated_bytes", 1024*1024*50,
                     nimcp.METRIC_CATEGORY_MEMORY)
```

### C API

```c
#include "utils/metrics/nimcp_metrics.h"

nimcp_metrics_collector_t metrics = nimcp_metrics_create();

// Record hierarchical metrics
nimcp_metrics_record_gauge(metrics, "hierarchical.num_regions", 5.0,
                            NIMCP_METRIC_CATEGORY_SYSTEM);

// Performance timer
uint64_t start = nimcp_metrics_timer_start(metrics, "forward_pass");
// ... do work ...
nimcp_metrics_timer_stop(metrics, "forward_pass", start,
                         NIMCP_METRIC_CATEGORY_PERFORMANCE);

// Complete hierarchical snapshot
nimcp_hierarchical_metrics_t brain_metrics = {
    .timestamp_ms = get_timestamp_ms(),
    .num_regions = 5,
    .num_layers = 4,
    .total_forward_passes = 1000,
    .avg_forward_time_ms = 12.5,
    .dopamine_level = 0.8,
    .acetylcholine_level = 0.6,
    .serotonin_level = 0.7
};
nimcp_metrics_record_hierarchical(metrics, &brain_metrics);
```

---

## Tableau Dashboard Templates

### Performance Dashboard

Recommended visualizations:
- **Line Chart**: `hierarchical.forward_passes` over time
- **Gauge**: `hierarchical.avg_forward_time_ms` (current)
- **Bar Chart**: `region.<name>.activations` by region
- **Histogram**: `hierarchical.forward_pass_latency_p95` distribution

### Learning Dashboard

Recommended visualizations:
- **Line Chart**: `hierarchical.avg_error` and `hierarchical.avg_accuracy` over time
- **Scatter Plot**: Error vs accuracy correlation
- **Heat Map**: Per-region learning rates
- **Area Chart**: `hierarchical.learning_updates` cumulative

### System Health Dashboard

Recommended visualizations:
- **Multi-line Chart**: Memory metrics over time
- **Gauge**: CPU and memory utilization
- **Bar Chart**: Thread pool statistics
- **Table**: Top resource-consuming regions

---

## PowerBI Report Templates

### Executive Summary

Key metrics to display:
- Total forward passes (KPI card)
- Average accuracy (KPI card)
- System uptime (KPI card)
- Active regions (gauge visual)
- Performance trend (line chart)

### Detailed Analysis

Metrics for deep dives:
- Region-level performance matrix
- Learning curve progression
- Memory utilization trends
- Network communication patterns

---

## Metric Collection Best Practices

1. **Sample Rate**: High-frequency metrics (>100 Hz) should be sampled
2. **Buffering**: Use automatic flushing for real-time dashboards
3. **Aggregation**: Pre-aggregate metrics where possible
4. **Retention**: Define retention policies for historical data
5. **Alerting**: Set thresholds on critical metrics
6. **Documentation**: Keep this catalog updated

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-04 | Initial comprehensive catalog |

---

## See Also

- [Metrics Implementation Guide](./METRICS_IMPLEMENTATION.md)
- [Tableau Integration](./METRICS_IMPLEMENTATION.md#tableau-integration)
- [PowerBI Integration](./METRICS_IMPLEMENTATION.md#powerbi-integration)
- [API Reference](../src/utils/metrics/nimcp_metrics.h)
