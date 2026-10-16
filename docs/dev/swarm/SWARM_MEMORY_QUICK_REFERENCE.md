# Swarm Memory Consolidation - Quick Reference

## Files
- **Header**: `include/swarm/nimcp_swarm_memory.h`
- **Implementation**: `src/swarm/nimcp_swarm_memory.c`

## Quick Start

```c
#include "swarm/nimcp_swarm_memory.h"

// 1. Create and initialize
NimcpSwarmMemory *memory = nimcp_swarm_memory_create(10000, 3);
nimcp_swarm_memory_init(memory, bio_ctx);

// 2. Register nodes
nimcp_swarm_memory_register_node(memory, "node1", 1000);

// 3. Store memory
char id[64];
nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC,
                         NIMCP_IMPORTANCE_HIGH, data, size, id);

// 4. Distribute
uint32_t replicas;
nimcp_swarm_memory_distribute(memory, id, &replicas);

// 5. Consolidate
uint32_t consolidated;
nimcp_swarm_memory_consolidate(memory, &consolidated);

// 6. Cleanup
nimcp_swarm_memory_destroy(memory);
```

## Memory Types

| Type | Description | Decay Rate |
|------|-------------|------------|
| `NIMCP_MEMORY_EPISODIC` | Events/experiences | Fast (1.5x) |
| `NIMCP_MEMORY_SEMANTIC` | Knowledge/patterns | Slow (0.3x) |
| `NIMCP_MEMORY_PROCEDURAL` | Learned behaviors | Normal |
| `NIMCP_MEMORY_THREAT` | Threat patterns | Slow (0.5x) |
| `NIMCP_MEMORY_SPATIAL` | Location/maps | Normal |

## Importance Levels

| Level | Value | Effect |
|-------|-------|--------|
| `NIMCP_IMPORTANCE_LOW` | 0 | Fast decay |
| `NIMCP_IMPORTANCE_MEDIUM` | 1 | Normal decay |
| `NIMCP_IMPORTANCE_HIGH` | 2 | Slow decay, shared to swarm |
| `NIMCP_IMPORTANCE_CRITICAL` | 3 | Very slow decay, high priority |

## Consolidation Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| `NIMCP_CONSOLIDATION_ACTIVE` | High priority | Critical updates |
| `NIMCP_CONSOLIDATION_PASSIVE` | Background | Regular maintenance |
| `NIMCP_CONSOLIDATION_SLEEP` | Offline | Deep consolidation |

## Key Operations

### Store Memory
```c
char memory_id[64];
NimcpResult result = nimcp_swarm_memory_store(
    memory,
    NIMCP_MEMORY_SEMANTIC,
    NIMCP_IMPORTANCE_HIGH,
    data_ptr,
    data_size,
    memory_id
);
```

### Retrieve Memory
```c
uint8_t buffer[1024];
NimcpResult result = nimcp_swarm_memory_retrieve(
    memory,
    memory_id,
    buffer,
    sizeof(buffer)
);
```

### Rehearse Memory (Strengthen)
```c
nimcp_swarm_memory_rehearse(memory, memory_id);
```

### Experience Replay
```c
// Schedule for replay
nimcp_swarm_memory_schedule_replay(memory, memory_id, priority);

// Execute replay cycle
uint32_t replays_done;
nimcp_swarm_memory_replay_cycle(memory, max_replays, &replays_done);
```

### Compression
```c
NimcpCompressedMemory compressed;
nimcp_swarm_memory_compress(memory, memory_id, &compressed);

// Later, decompress
uint8_t decompressed[1024];
nimcp_swarm_memory_decompress(memory, &compressed, 
                               decompressed, sizeof(decompressed));
```

### Distribution
```c
uint32_t replicas_created;
nimcp_swarm_memory_distribute(memory, memory_id, &replicas_created);

// Verify consensus
bool has_consensus;
nimcp_swarm_memory_verify_consensus(memory, memory_id, &has_consensus);
```

### Consolidation Window
```c
// Configure
NimcpConsolidationWindow window = {
    .mode = NIMCP_CONSOLIDATION_PASSIVE,
    .window_duration_ms = 300000,  // 5 minutes
    .max_memories_per_window = 100,
    .activity_threshold = 0.3f,
    .auto_schedule = true
};
nimcp_swarm_memory_configure_consolidation(memory, &window);

// Start consolidation
nimcp_swarm_memory_start_consolidation(memory, NIMCP_CONSOLIDATION_PASSIVE);

// Execute
uint32_t consolidated;
nimcp_swarm_memory_consolidate(memory, &consolidated);
```

### Bio-Async Integration
```c
// Process incoming message
nimcp_swarm_memory_process_message(memory, &msg);

// Send memory to node
nimcp_swarm_memory_send_memory(memory, memory_id, "node2");

// Request memory from peer
nimcp_swarm_memory_request_memory(memory, memory_id, "node1");

// Broadcast consolidation signal
nimcp_swarm_memory_broadcast_consolidation(memory, mode);
```

### Statistics
```c
// Get statistics
NimcpMemoryStatistics stats;
nimcp_swarm_memory_get_statistics(memory, &stats);

// Get health score
float health = nimcp_swarm_memory_get_health_score(memory);

// Print status
nimcp_swarm_memory_print_status(memory, true);  // verbose
```

## Default Configuration

```c
// Capacity
max_memory_capacity = user_specified

// Replication
replication_factor = 3
consensus_threshold = 0.67

// Novelty & Replay
novelty_threshold = 0.6
replay_probability = 0.1

// Compression
auto_compression = true
compression_target = 0.5

// Distribution
auto_distribution = true

// Forgetting Curves
decay_rate = 0.0001
rehearsal_boost = 0.1
half_life_ms = 86400000  // 1 day
```

## Forgetting Curve Formula

```
strength(t) = initial_strength * exp(-decay_rate * importance_modifier * t / 1000)

where:
  importance_modifier = 1.0 - (importance / CRITICAL) * 0.5
  decay_modifier = importance_factor * rehearsal_factor
  rehearsal_factor = 1.0 / (1.0 + log(1.0 + rehearsal_count))
```

## Replay Priority Formula

```
priority = (novelty * 0.4) + (importance * 0.4) + (recency * 0.2)

where:
  novelty = memory.novelty_score (0.0-1.0)
  importance = memory.importance / CRITICAL
  recency = exp(-age / 3600000)  // decay over hours
```

## Health Score Formula

```
health = (node_health * 0.4) + (replication_health * 0.4) + (consolidation_health * 0.2)

where:
  node_health = active_nodes / replication_factor
  replication_health = distributed_memories / total_memories
  consolidation_health = consolidation_count > 0 ? 1.0 : 0.5
```

## Bio-Async Message Types

| Type | Direction | Payload | Purpose |
|------|-----------|---------|---------|
| `MEMORY_SHARE` | Node → Swarm | Memory data | Share memory |
| `MEMORY_REQUEST` | Node → Node | Memory ID | Request memory |
| `MEMORY_RESPONSE` | Node → Node | Memory data | Respond to request |
| `CONSOLIDATION_SIGNAL` | Coordinator → Swarm | Mode | Trigger consolidation |
| `CONSENSUS_REQUEST` | Node → Swarm | Memory ID | Verify consensus |
| `SYNC_REQUEST` | Node → Node | None | Sync memories |

## Performance Tips

1. **Batch operations**: Use consolidation windows for batch processing
2. **Compression**: Enable auto-compression for network efficiency
3. **Replay probability**: Adjust based on system load (0.05-0.2)
4. **Replication factor**: Balance reliability vs overhead (2-5)
5. **Forgetting**: Run periodically to free memory (hourly)
6. **Node capacity**: Size appropriately for expected load

## Common Patterns

### Periodic Consolidation
```c
// Every 5 minutes
if (should_consolidate()) {
    nimcp_swarm_memory_start_consolidation(memory, NIMCP_CONSOLIDATION_PASSIVE);
    uint32_t consolidated;
    nimcp_swarm_memory_consolidate(memory, &consolidated);
}
```

### High-Priority Memory
```c
// Critical threat detected
nimcp_swarm_memory_store(memory, NIMCP_MEMORY_THREAT,
                         NIMCP_IMPORTANCE_CRITICAL, threat_data, size, id);
nimcp_swarm_memory_schedule_replay(memory, id, 1.0f);
nimcp_swarm_memory_distribute(memory, id, &replicas);
```

### Memory Rehearsal Loop
```c
// Strengthen important memories
for (int i = 0; i < important_count; i++) {
    nimcp_swarm_memory_rehearse(memory, important_ids[i]);
}
```

### Node Failure Recovery
```c
// Unregister failed node
nimcp_swarm_memory_unregister_node(memory, "failed_node");

// Re-distribute affected memories
for (each affected memory) {
    nimcp_swarm_memory_distribute(memory, memory_id, &replicas);
}
```

## Error Handling

```c
NimcpResult result = nimcp_swarm_memory_store(...);
if (result != NIMCP_OK) {
    switch (result) {
        case NIMCP_ERROR_NOT_INITIALIZED:
            // Initialize first
            break;
        case NIMCP_ERROR_MEMORY_ALLOCATION:
            // Out of memory
            break;
        case NIMCP_ERROR_INVALID_ARGUMENT:
            // Check parameters
            break;
        default:
            // Other error
            break;
    }
}
```

## Integration Checklist

- [ ] Include header file
- [ ] Create swarm memory system
- [ ] Initialize with bio-async context
- [ ] Register hippocampus nodes
- [ ] Configure forgetting curves (optional)
- [ ] Configure consolidation window (optional)
- [ ] Implement message handler
- [ ] Start periodic consolidation
- [ ] Monitor health score
- [ ] Handle node failures

## See Also

- `SWARM_MEMORY_CONSOLIDATION_SUMMARY.md` - Full implementation details
- `SWARM_GATEWAY_GUIDE.md` - Swarm gateway integration
- `BIO_ASYNC_INTEGRATION_SUMMARY.md` - Bio-async messaging

---

*Quick Reference v1.0*
*Last Updated: 2025-12-08*
