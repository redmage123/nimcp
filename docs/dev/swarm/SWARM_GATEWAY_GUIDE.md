# NIMCP Server-to-Swarm Gateway Guide

## Overview

The **NIMCP Server-to-Swarm Gateway** enables large NIMCP server brains to communicate with drone swarms, providing:

- **Learning Synchronization**: Server trains on aggregated data, propagates weight updates to swarms
- **Mission Control**: Send mission parameters, formation commands, and operational directives
- **Telemetry Aggregation**: Receive and aggregate telemetry from multiple swarms
- **P2P Propagation**: Efficient message distribution via peer-to-peer relay within swarms
- **Health Monitoring**: Track swarm connectivity, drone status, and communication quality

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Server NIMCP Brain                        │
│              (10,000+ neurons, full training)                │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
          ┌──────────────────────────────┐
          │   Swarm Gateway (This API)    │
          │  - Learning Sync              │
          │  - Mission Control            │
          │  - Telemetry Aggregation      │
          └──────────┬───────────┬────────┘
                     │           │
        ┌────────────┘           └────────────┐
        ▼                                     ▼
┌───────────────┐                     ┌───────────────┐
│  Swarm ALPHA  │                     │  Swarm BETA   │
│  10 Drones    │                     │  10 Drones    │
│  P2P Network  │                     │  P2P Network  │
└───────────────┘                     └───────────────┘
```

## Key Features

### 1. Learning Propagation

The gateway implements efficient learning synchronization:

- **Server Training**: Server brain trains on aggregated swarm data
- **Delta Encoding**: Only significant weight changes are transmitted
- **Compression**: Typical 10-100x compression ratios
- **P2P Distribution**: Send to one drone, swarm propagates internally
- **Incremental Updates**: Drones apply deltas to their existing weights

**Example:**
```c
learning_update_t update;
swarm_gateway_create_learning_update(gateway, &update);
// update contains compressed deltas

int swarms_synced = swarm_gateway_send_learning_update(gateway, NULL, &update);
// Broadcast to all swarms (NULL = broadcast)

swarm_gateway_free_learning_update(&update);
```

### 2. Mission Control

Send missions and tactical commands to swarms:

**Mission Parameters:**
```c
mission_params_t mission = {
    .mission_id = "RECON_001",
    .mission_type = 1,  // Reconnaissance
    .target_coordinates = {100.0f, 200.0f, 50.0f},
    .search_radius = 500.0f,
    .duration_ms = 300000,  // 5 minutes
    .num_objectives = 3
};

swarm_gateway_send_mission(gateway, "ALPHA_SQUAD", &mission);
```

**Formation Commands:**
```c
formation_cmd_t formation = {
    .formation_type = 2,  // Diamond
    .center_position = {50.0f, 50.0f, 100.0f},
    .spacing = 10.0f,
    .orientation = 45.0f,
    .transition_time_ms = 5000
};

swarm_gateway_send_formation_cmd(gateway, "BETA_SQUAD", &formation);
```

### 3. Telemetry Aggregation

Receive comprehensive telemetry from swarms:

```c
swarm_telemetry_t telemetry;
if (swarm_gateway_receive_telemetry(gateway, "ALPHA_SQUAD", &telemetry) == 0) {
    printf("Active Drones: %u/%u\n",
           telemetry.num_responsive, telemetry.num_drones);
    printf("Battery: %.1f%%\n", telemetry.avg_battery_level);
    printf("Formation Coherence: %.2f\n", telemetry.formation_coherence);
    printf("Mission Progress: %.1f%%\n", telemetry.mission_progress * 100.0f);
}
```

**Telemetry includes:**
- Drone counts (total, active, failed)
- Resource usage (battery, CPU, memory)
- Formation quality metrics
- Mission progress tracking
- Center of mass and bounding box
- Threat detection counts
- Communication health

### 4. Health Monitoring

Track swarm health and connectivity:

```c
swarm_health_t health;
swarm_gateway_get_swarm_status(gateway, "ALPHA_SQUAD", &health);

printf("Status: %s\n", swarm_gateway_status_to_string(health.status));
printf("Health Score: %.1f%%\n", health.overall_health * 100.0f);
printf("Latency: %.1f ms\n", health.latency_ms);
printf("Packets: sent=%lu, received=%lu\n",
       health.packets_sent, health.packets_received);
```

**Status States:**
- `SWARM_STATUS_CONNECTED` - Normal operation
- `SWARM_STATUS_DEGRADED` - Partial connectivity
- `SWARM_STATUS_TIMEOUT` - No contact for timeout period
- `SWARM_STATUS_CONNECTING` - Connection in progress
- `SWARM_STATUS_DISCONNECTED` - Not connected

## Message Types

The gateway supports 8 message types:

| Type | Purpose | Use Case |
|------|---------|----------|
| `GATEWAY_MSG_LEARNING_UPDATE` | Weight deltas from server | Propagate trained weights |
| `GATEWAY_MSG_MISSION_PARAMS` | Mission objectives | Assign tasks to swarms |
| `GATEWAY_MSG_THREAT_INTEL` | Threat information | Share detected threats |
| `GATEWAY_MSG_FORMATION_CMD` | Formation patterns | Coordinate spatial arrangement |
| `GATEWAY_MSG_RECALL` | Return to base | Emergency recall |
| `GATEWAY_MSG_NEUROMOD_OVERRIDE` | Behavioral control | Override neuromodulators |
| `GATEWAY_MSG_HEARTBEAT` | Keep-alive signal | Maintain connection |
| `GATEWAY_MSG_SYNC_REQUEST` | Sync request | Request swarm synchronization |

## Usage Guide

### Basic Setup

```c
#include "swarm/nimcp_swarm_gateway.h"

// 1. Create server brain
brain_config_t brain_config = {
    .num_neurons = 10000,
    .enable_plasticity = true,
    .learning_rate = 0.01f
};
brain_t* server_brain = brain_create(&brain_config);

// 2. Configure gateway
swarm_gateway_config_t gateway_config = {
    .gateway_name = "MISSION_CONTROL",
    .max_swarms = 10,
    .broadcast_interval_ms = 1000,
    .timeout_ms = 5000,
    .enable_learning_sync = true,
    .enable_mission_control = true,
    .enable_telemetry = true
};

// 3. Create gateway
swarm_gateway_t* gateway = swarm_gateway_create(server_brain, &gateway_config);

// 4. Connect to swarms
swarm_gateway_connect_swarm(gateway, "ALPHA_SQUAD", "192.168.1.100:8000");
swarm_gateway_connect_swarm(gateway, "BETA_SQUAD", "192.168.1.101:8000");
```

### Event-Driven Operation

Register callbacks for asynchronous events:

```c
// Telemetry callback
void on_telemetry(const char* swarm_id,
                  const swarm_telemetry_t* telemetry,
                  void* user_data) {
    printf("Telemetry from %s: battery=%.1f%%\n",
           swarm_id, telemetry->avg_battery_level);
}

// Event callback
void on_swarm_event(const char* swarm_id,
                    const char* event_type,
                    const void* event_data,
                    void* user_data) {
    printf("Swarm event: %s - %s\n", swarm_id, event_type);
}

// Register callbacks
swarm_gateway_register_telemetry_callback(gateway, on_telemetry, NULL);
swarm_gateway_register_event_callback(gateway, on_swarm_event, NULL);
```

### Main Processing Loop

```c
while (running) {
    // Process gateway operations
    // - Checks timeouts
    // - Sends heartbeats
    // - Receives telemetry
    // - Triggers callbacks
    int events = swarm_gateway_process(gateway, 100);

    // Periodic learning sync
    if (should_sync_learning()) {
        swarm_gateway_sync_learning(gateway);
    }

    // Aggregate data to server
    if (should_aggregate()) {
        swarm_gateway_aggregate_to_server(gateway);
    }
}
```

### Advanced Operations

**Threat Intelligence Broadcasting:**
```c
threat_intel_t threat = {
    .threat_id = 42,
    .threat_level = 8,
    .position = {100.0f, 200.0f, 0.0f},
    .velocity = {-5.0f, -3.0f, 0.0f},
    .threat_type = "HOSTILE_AIRCRAFT"
};

// Broadcast to all swarms (NULL = broadcast)
int swarms_notified = swarm_gateway_send_threat_intel(gateway, NULL, &threat);
```

**Neuromodulator Override:**
```c
neuromod_override_t override = {
    .modulator_type = DOPAMINE,
    .override_value = 0.8f,  // High reward expectation
    .duration_ms = 60000,    // 1 minute
    .apply_to_all = true     // All drones in swarm
};

swarm_gateway_send_neuromod_override(gateway, "ALPHA_SQUAD", &override);
```

**Emergency Recall:**
```c
// Emergency recall (highest priority)
swarm_gateway_send_recall(gateway, "ALPHA_SQUAD", true);

// Normal recall
swarm_gateway_send_recall(gateway, "BETA_SQUAD", false);
```

## Configuration Options

### Gateway Configuration

```c
typedef struct {
    char gateway_name[64];           // Gateway identifier
    uint32_t max_swarms;             // Max connected swarms (1-1000)
    uint32_t broadcast_interval_ms;  // Update broadcast rate (ms)
    uint32_t timeout_ms;             // Swarm timeout threshold (ms)
    bool enable_learning_sync;       // Push learning updates
    bool enable_mission_control;     // Accept mission commands
    bool enable_telemetry;           // Receive swarm telemetry
} swarm_gateway_config_t;
```

**Recommended Settings:**

| Scenario | Max Swarms | Broadcast Interval | Timeout |
|----------|------------|-------------------|---------|
| Low-latency tactical | 5 | 500ms | 2000ms |
| Standard operations | 10 | 1000ms | 5000ms |
| Large-scale coordination | 50 | 2000ms | 10000ms |
| Resource-constrained | 3 | 5000ms | 15000ms |

## P2P Propagation

The gateway uses **Peer-to-Peer (P2P) propagation** for efficiency:

### How It Works

1. **Gateway → Relay Drone**: Gateway sends message to designated relay drone in swarm
2. **Relay → Peers**: Relay drone propagates to neighboring drones
3. **Peers → Peers**: Message spreads through swarm via gossip protocol
4. **Acknowledgment**: Swarm confirms receipt back to gateway

### Benefits

- **Bandwidth Efficiency**: Gateway sends once per swarm, not once per drone
- **Scalability**: O(1) messages from gateway regardless of swarm size
- **Resilience**: Message propagates even if relay drone fails
- **Speed**: Parallel propagation within swarm is faster than sequential

### Example

For a swarm of 100 drones:
- **Without P2P**: Gateway sends 100 messages
- **With P2P**: Gateway sends 1 message, swarm propagates internally

## Learning Synchronization Details

### Delta Encoding

The gateway compresses learning updates using delta encoding:

```
Initial Sync (t=0):
  Full weights: [w1, w2, w3, ..., w10000]
  Size: 10,000 floats = 40 KB

Update (t=1):
  Changed weights: [w42: Δ+0.01, w157: Δ-0.005, w983: Δ+0.03]
  Size: 3 deltas = 12 bytes
  Compression: 3,333x
```

### Synchronization Process

1. **Server Training**: Server brain learns from aggregated swarm experiences
2. **Delta Computation**: Gateway computes weight changes since last sync
3. **Compression**: Only significant deltas (|Δ| > threshold) are included
4. **Broadcast**: Compressed update sent to swarms via P2P
5. **Application**: Each drone applies deltas to its local weights
6. **Verification**: Swarms acknowledge successful update

### Configuration

```c
// Automatic sync every 10 broadcast intervals
uint32_t sync_interval = config.broadcast_interval_ms * 10;

// Manual sync
swarm_gateway_sync_learning(gateway);

// Create custom update
learning_update_t update;
swarm_gateway_create_learning_update(gateway, &update);
// ... customize ...
swarm_gateway_send_learning_update(gateway, "ALPHA_SQUAD", &update);
swarm_gateway_free_learning_update(&update);
```

## Statistics and Monitoring

### Gateway Statistics

```c
uint32_t num_swarms, total_drones;
uint64_t msgs_sent, msgs_received;

swarm_gateway_get_stats(gateway, &num_swarms, &total_drones,
                       &msgs_sent, &msgs_received);

printf("Connected Swarms: %u\n", num_swarms);
printf("Total Drones: %u\n", total_drones);
printf("Messages: sent=%lu, received=%lu\n", msgs_sent, msgs_received);
```

### Per-Swarm Health

```c
swarm_health_t health;
swarm_gateway_get_swarm_status(gateway, "ALPHA_SQUAD", &health);

// Health score combines:
// - Drone availability (active/total)
// - Connection quality (connected, degraded, timeout)
float health_score = health.overall_health;  // 0.0 - 1.0
```

## BIO-Async Integration

The gateway supports NIMCP's BIO-async message passing:

```c
// Gateway automatically detects if server brain has BIO-async enabled
if (gateway->bio_async_enabled) {
    // Uses asynchronous message passing
    // Inbox handler processes async updates
} else {
    // Falls back to synchronous operation
}
```

### BIO-Async Benefits

- **Non-blocking Operations**: Gateway doesn't block on network I/O
- **Concurrent Processing**: Multiple swarms processed in parallel
- **Efficient Resource Usage**: Callbacks triggered only when data arrives

## Error Handling

All gateway functions return status codes:

```c
int result = swarm_gateway_send_mission(gateway, "ALPHA_SQUAD", &mission);

if (result < 0) {
    switch (result) {
        case -EINVAL:  // Invalid arguments
            fprintf(stderr, "Invalid mission parameters\n");
            break;
        case -ENOENT:  // Swarm not found
            fprintf(stderr, "Swarm 'ALPHA_SQUAD' not connected\n");
            break;
        case -ENOTCONN:  // Not connected
            fprintf(stderr, "Swarm not in connected state\n");
            break;
        case -EPERM:  // Operation not permitted
            fprintf(stderr, "Mission control not enabled\n");
            break;
        default:
            fprintf(stderr, "Unknown error: %d\n", result);
    }
}
```

## Best Practices

### 1. Connection Management

- **Heartbeats**: Gateway sends automatic heartbeats every 2x broadcast interval
- **Timeouts**: Swarms marked as timeout after no contact for `timeout_ms`
- **Reconnection**: Monitor event callbacks and reconnect on timeout

### 2. Learning Sync

- **Frequency**: Sync every 10-100 seconds for balance between freshness and bandwidth
- **Selective Sync**: Send updates only to swarms that need them
- **Compression**: Use delta encoding for 10-1000x bandwidth savings

### 3. Message Priority

- **High Priority**: RECALL, THREAT_INTEL (immediate transmission)
- **Medium Priority**: MISSION_PARAMS, FORMATION_CMD (acknowledged)
- **Low Priority**: LEARNING_UPDATE, HEARTBEAT (best-effort)

### 4. Telemetry Processing

- **Callbacks**: Use async callbacks for real-time telemetry
- **Aggregation**: Call `aggregate_to_server()` periodically for macro decisions
- **Rate Limiting**: Process telemetry at sustainable rate (e.g., 1 Hz per swarm)

### 5. Resource Management

- **Cleanup**: Always call `swarm_gateway_destroy()` to free resources
- **Memory**: Free learning updates with `swarm_gateway_free_learning_update()`
- **Threads**: Gateway is thread-safe via internal mutex

## Example Use Cases

### Tactical Reconnaissance Mission

```c
// Send reconnaissance mission to multiple swarms
for (int i = 0; i < num_swarms; i++) {
    mission_params_t mission = {
        .mission_id = mission_ids[i],
        .mission_type = RECON,
        .target_coordinates = targets[i],
        .search_radius = 1000.0f,
        .duration_ms = 600000  // 10 minutes
    };
    swarm_gateway_send_mission(gateway, swarm_ids[i], &mission);
}
```

### Coordinated Threat Response

```c
// Broadcast threat to all swarms
threat_intel_t threat = { /* ... */ };
swarm_gateway_send_threat_intel(gateway, NULL, &threat);

// Command closest swarm to intercept
formation_cmd_t intercept = {
    .formation_type = INTERCEPT,
    .center_position = threat.position,
    .spacing = 50.0f
};
swarm_gateway_send_formation_cmd(gateway, closest_swarm, &intercept);
```

### Distributed Learning Pipeline

```c
while (training) {
    // 1. Collect telemetry from all swarms
    swarm_gateway_aggregate_to_server(gateway);

    // 2. Server brain trains on aggregated data
    brain_train(server_brain, aggregated_data);

    // 3. Sync updated weights to swarms
    swarm_gateway_sync_learning(gateway);

    // 4. Monitor convergence
    if (check_convergence()) break;
}
```

## Performance Characteristics

| Metric | Value |
|--------|-------|
| Max Swarms | 1000 (configurable) |
| Swarm Connection Time | ~100ms |
| Message Latency | 10-100ms (network dependent) |
| Learning Update Size | 10-1000 bytes (compressed) |
| Telemetry Frequency | 1-10 Hz per swarm |
| Throughput | 10,000+ messages/sec |

## API Reference Summary

### Core Functions
- `swarm_gateway_create()` - Create gateway
- `swarm_gateway_destroy()` - Cleanup
- `swarm_gateway_connect_swarm()` - Connect to swarm
- `swarm_gateway_disconnect_swarm()` - Disconnect
- `swarm_gateway_process()` - Main processing loop

### Message Transmission
- `swarm_gateway_broadcast_update()` - Broadcast to all
- `swarm_gateway_send_to_swarm()` - Send to specific swarm
- `swarm_gateway_send_mission()` - Mission parameters
- `swarm_gateway_send_learning_update()` - Weight updates
- `swarm_gateway_send_threat_intel()` - Threat information
- `swarm_gateway_send_formation_cmd()` - Formation commands
- `swarm_gateway_send_recall()` - Return to base
- `swarm_gateway_send_neuromod_override()` - Behavioral control

### Telemetry & Status
- `swarm_gateway_receive_telemetry()` - Get telemetry
- `swarm_gateway_get_swarm_status()` - Get health status
- `swarm_gateway_get_connected_swarms()` - List swarms
- `swarm_gateway_register_telemetry_callback()` - Register callback
- `swarm_gateway_register_event_callback()` - Register event handler

### Maintenance
- `swarm_gateway_sync_learning()` - Manual sync
- `swarm_gateway_aggregate_to_server()` - Aggregate data
- `swarm_gateway_get_stats()` - Get statistics

## Troubleshooting

### Problem: Swarm Timeout

**Symptoms**: Swarm status shows `SWARM_STATUS_TIMEOUT`

**Solutions**:
- Check network connectivity
- Increase `timeout_ms` in config
- Verify swarm endpoint is correct
- Check swarm is running and responsive

### Problem: Learning Updates Not Applied

**Symptoms**: Drones don't reflect server brain changes

**Solutions**:
- Verify `enable_learning_sync` is true
- Check learning update creation succeeded
- Ensure swarms are in CONNECTED state
- Verify P2P propagation is working

### Problem: High Latency

**Symptoms**: Messages take too long to reach swarms

**Solutions**:
- Reduce `broadcast_interval_ms`
- Check network congestion
- Use P2P propagation (default)
- Optimize message payload size

## Conclusion

The NIMCP Server-to-Swarm Gateway provides a powerful, scalable interface for coordinating large-scale drone swarms with a central server brain. Key advantages:

- **Efficient**: P2P propagation scales to large swarms
- **Intelligent**: Learning synchronization enables swarm-wide adaptation
- **Robust**: Timeout detection and health monitoring
- **Flexible**: Support for missions, formations, and behavioral control

For more examples, see `examples/swarm_gateway_demo.c`.
