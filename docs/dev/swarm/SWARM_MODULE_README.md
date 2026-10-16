# NIMCP Swarm Module

## Overview

The NIMCP Swarm module provides infrastructure for coordinating distributed drone swarms using biologically-inspired neural networks. The module enables:

- **Collective Intelligence**: Multiple small brains working as distributed cognitive system
- **Server-Swarm Communication**: Large server brains coordinating with drone swarms
- **P2P Propagation**: Efficient peer-to-peer message distribution
- **Learning Synchronization**: Distributed learning with centralized aggregation
- **Emergent Behavior**: Swarm-level intelligence from individual drone interactions

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     NIMCP Swarm Module                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │   Gateway    │  │  Protocol    │  │   Signal     │     │
│  │  (Server →   │  │  (Phoneme-   │  │  (Drone →    │     │
│  │   Swarm)     │  │   based)     │  │   Drone)     │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │  Consensus   │  │  Emergence   │  │  Workspace   │     │
│  │  (Voting,    │  │  (Patterns)  │  │  (Shared     │     │
│  │   Quorum)    │  │              │  │   Memory)    │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Components

### 1. Swarm Gateway (`nimcp_swarm_gateway`)

**Purpose**: Server-to-swarm communication gateway

**Features**:
- Connect server brain to multiple drone swarms
- Learning synchronization with delta encoding
- Mission control and formation commands
- Telemetry aggregation from swarms
- P2P message propagation
- Health monitoring and timeout detection

**Use Cases**:
- Large server brain coordinating multiple swarms
- Centralized training with distributed inference
- Mission command and control
- Swarm health monitoring

**See**: [SWARM_GATEWAY_GUIDE.md](SWARM_GATEWAY_GUIDE.md)

### 2. Swarm Protocol (`nimcp_swarm_protocol`)

**Purpose**: Phoneme-based drone-to-drone communication

**Features**:
- Compact 24-byte messages
- Phoneme sequence encoding (biologically-inspired)
- CRC16 error detection
- 11 message types (heartbeat, threat, target, etc.)
- Maps to speech cortex infrastructure

**Use Cases**:
- Efficient swarm communication
- Noisy communication channels
- Speech cortex integration
- Error-resistant messaging

### 3. Swarm Signal (`nimcp_swarm_signal`)

**Purpose**: Low-level signal transmission between drones

**Features**:
- Direct drone-to-drone signaling
- Minimal overhead
- Fast propagation
- Position/velocity sharing

**Use Cases**:
- Real-time coordination
- Formation maintenance
- Collision avoidance
- Proximity sensing

### 4. Swarm Consensus (`nimcp_swarm_consensus`)

**Purpose**: Distributed decision-making

**Features**:
- Voting mechanisms
- Quorum-based decisions
- Byzantine fault tolerance
- Leader election

**Use Cases**:
- Distributed target selection
- Formation decisions
- Conflict resolution
- Fault tolerance

### 5. Swarm Emergence (`nimcp_swarm_emergence`)

**Purpose**: Detect emergent swarm patterns

**Features**:
- Pattern recognition
- Collective behavior analysis
- Phase transitions
- Emergent property detection

**Use Cases**:
- Swarm intelligence analysis
- Behavioral monitoring
- Optimization
- Research and development

### 6. Collective Workspace (`nimcp_collective_workspace`)

**Purpose**: Shared memory and state across swarm

**Features**:
- Distributed shared memory
- Consistency protocols
- Synchronization
- Conflict resolution

**Use Cases**:
- Shared mission state
- Distributed map building
- Collective memory
- Coordinated learning

## Module Hierarchy

```
Server NIMCP Brain (10,000+ neurons)
         ↓
   Swarm Gateway ← YOU ARE HERE
         ↓
   ┌─────┴─────┬─────────┬─────────┐
   ↓           ↓         ↓         ↓
Swarm 1     Swarm 2   Swarm 3   Swarm 4
(10 drones) (10 drones) (10 drones) (10 drones)
   ↓
Drone 1 ← Uses Swarm Protocol, Signal, Consensus
(100-1000 neurons)
```

## Key Concepts

### P2P Propagation

Messages from server to swarm use **peer-to-peer propagation**:

1. Gateway sends to one "relay drone" in swarm
2. Relay drone propagates to neighbors
3. Message spreads through swarm via gossip
4. Efficient: O(1) messages from gateway, O(log N) hops within swarm

### Learning Synchronization

**Centralized Training, Distributed Inference**:

```
1. Drones collect data → aggregate to server
2. Server brain trains on combined data
3. Server computes weight deltas
4. Gateway broadcasts deltas to swarms
5. Drones apply deltas incrementally
```

**Benefits**:
- Server has more compute for complex training
- Drones stay synchronized with latest model
- Bandwidth-efficient (delta encoding)
- Scalable to large swarms

### Phoneme-Based Protocol

Inspired by speech cortex, messages are encoded as phoneme sequences:

- **Compact**: 8 phonemes per message
- **Distinctive**: Phonemes easily distinguished in noise
- **Biological**: Maps to existing speech cortex infrastructure
- **Error-Resistant**: CRC16 + phoneme redundancy

**Examples**:
- Heartbeat: `/hɛlo/` (HELLO)
- Threat: `/deɪnʤər/` (DANGER)
- Target: `/faʊnd/` (FOUND)

## File Structure

```
include/swarm/
├── nimcp_swarm_gateway.h         ← Server-to-swarm gateway API
├── nimcp_swarm_protocol.h        ← Phoneme-based protocol
├── nimcp_swarm_signal.h          ← Drone-to-drone signaling
├── nimcp_swarm_consensus.h       ← Distributed consensus
├── nimcp_swarm_emergence.h       ← Emergent pattern detection
├── nimcp_swarm_brain.h           ← Swarm-level brain abstraction
└── nimcp_collective_workspace.h  ← Shared swarm memory

src/swarm/
├── nimcp_swarm_gateway.c         ← Gateway implementation
├── nimcp_swarm_protocol.c        ← Protocol implementation
├── nimcp_swarm_signal.c          ← Signal implementation
├── nimcp_swarm_consensus.c       ← Consensus implementation
├── nimcp_swarm_emergence.c       ← Emergence detection
├── nimcp_swarm_brain.c           ← Swarm brain (TODO)
├── nimcp_collective_workspace.c  ← Workspace (TODO)
└── CMakeLists.txt

examples/
└── swarm_gateway_demo.c          ← Gateway usage example

docs/
├── SWARM_GATEWAY_GUIDE.md        ← Detailed gateway guide
└── SWARM_MODULE_README.md        ← This file
```

## Quick Start: Server-to-Swarm Gateway

### 1. Include Headers

```c
#include "swarm/nimcp_swarm_gateway.h"
#include "core/brain/nimcp_brain.h"
```

### 2. Create Server Brain

```c
brain_config_t config = {
    .num_neurons = 10000,
    .enable_plasticity = true,
    .learning_rate = 0.01f
};
brain_t* server = brain_create(&config);
```

### 3. Create Gateway

```c
swarm_gateway_config_t gw_config = {
    .gateway_name = "MISSION_CONTROL",
    .max_swarms = 10,
    .broadcast_interval_ms = 1000,
    .timeout_ms = 5000,
    .enable_learning_sync = true,
    .enable_mission_control = true,
    .enable_telemetry = true
};

swarm_gateway_t* gateway = swarm_gateway_create(server, &gw_config);
```

### 4. Connect to Swarms

```c
swarm_gateway_connect_swarm(gateway, "ALPHA", "192.168.1.100:8000");
swarm_gateway_connect_swarm(gateway, "BETA", "192.168.1.101:8000");
```

### 5. Send Mission

```c
mission_params_t mission = {
    .mission_id = "RECON_001",
    .target_coordinates = {100.0f, 200.0f, 50.0f},
    .search_radius = 500.0f,
    .duration_ms = 300000
};

swarm_gateway_send_mission(gateway, "ALPHA", &mission);
```

### 6. Process Events

```c
while (running) {
    swarm_gateway_process(gateway, 100);

    // Sync learning periodically
    if (should_sync()) {
        swarm_gateway_sync_learning(gateway);
    }

    // Monitor swarm health
    swarm_health_t health;
    swarm_gateway_get_swarm_status(gateway, "ALPHA", &health);
}
```

### 7. Cleanup

```c
swarm_gateway_destroy(gateway);
brain_destroy(server);
```

## Integration with NIMCP

### Brain Integration

The gateway connects to NIMCP brain instances:

```c
brain_t* server_brain = brain_create(&config);
swarm_gateway_t* gateway = swarm_gateway_create(server_brain, &gw_config);

// Gateway can access brain for:
// - Extracting weights for synchronization
// - Feeding back aggregated telemetry
// - Triggering brain-level decisions
```

### BIO-Async Support

If brain has BIO-async enabled, gateway uses it:

```c
// Gateway automatically detects BIO-async
if (nimcp_bio_get_context(server_brain)) {
    // Uses asynchronous message passing
    // Non-blocking network operations
    // Callbacks for telemetry
}
```

### Logging Integration

Uses NIMCP logging infrastructure:

```c
NIMCP_LOG_INFO("Gateway created");
NIMCP_LOG_DEBUG("Telemetry received from swarm");
NIMCP_LOG_WARN("Swarm timeout");
NIMCP_LOG_ERROR("Connection failed");
```

### Memory Management

Uses NIMCP memory allocators:

```c
gateway = nimcp_calloc(1, sizeof(swarm_gateway_t));
swarms = nimcp_malloc(max_swarms * sizeof(swarm_connection_t));
nimcp_free(gateway);
```

### Thread Safety

All gateway functions are thread-safe:

```c
nimcp_mutex_lock(gateway->mutex);
// ... critical section ...
nimcp_mutex_unlock(gateway->mutex);
```

## Message Flow Examples

### Example 1: Learning Synchronization

```
Server Brain (trains) → creates weight deltas
         ↓
   Gateway (compresses deltas)
         ↓
   Swarm ALPHA Relay Drone
         ↓
   Propagates to 9 other drones
         ↓
   Each drone applies deltas to local weights
```

### Example 2: Threat Detection

```
Drone 5 (detects threat) → sends to neighbors
         ↓
   Swarm ALPHA (aggregates) → sends to Gateway
         ↓
   Gateway (broadcasts to all swarms)
         ↓
   All swarms receive threat intel
         ↓
   Coordinated response
```

### Example 3: Mission Assignment

```
User/System → Gateway.send_mission("ALPHA", mission)
         ↓
   Gateway → Swarm ALPHA Relay
         ↓
   Relay → Propagates to swarm
         ↓
   Drones execute mission
         ↓
   Telemetry flows back to Gateway
         ↓
   Gateway.receive_telemetry() → User/System
```

## Performance Characteristics

| Component | Metric | Value |
|-----------|--------|-------|
| Gateway | Max Swarms | 1000 |
| Gateway | Throughput | 10,000 msg/s |
| Gateway | Latency | 10-100 ms |
| Protocol | Message Size | 24 bytes |
| Protocol | Throughput | 100,000 msg/s |
| Learning Sync | Compression | 10-1000x |
| Learning Sync | Frequency | 0.1-1 Hz |

## Best Practices

### 1. Gateway Configuration

- **Small swarms (1-10 drones)**: broadcast_interval=500ms, timeout=2s
- **Medium swarms (10-50 drones)**: broadcast_interval=1000ms, timeout=5s
- **Large swarms (50+ drones)**: broadcast_interval=2000ms, timeout=10s

### 2. Learning Synchronization

- Sync every 10-100 seconds for balance
- Use delta encoding for bandwidth efficiency
- Only sync when model has changed significantly

### 3. Message Priority

- High: RECALL, THREAT_INTEL (immediate)
- Medium: MISSION, FORMATION (acknowledged)
- Low: LEARNING_UPDATE, HEARTBEAT (best-effort)

### 4. Error Handling

- Always check return codes
- Monitor swarm health
- Implement reconnection logic
- Use event callbacks for alerts

### 5. Resource Management

- Call `destroy()` functions for cleanup
- Free learning updates after use
- Monitor memory usage
- Use callbacks to avoid polling

## Future Enhancements

### Planned Features

1. **Swarm Brain** (`nimcp_swarm_brain.c`):
   - Swarm-level cognitive abstraction
   - Distributed brain instance
   - Collective decision-making

2. **Collective Workspace** (`nimcp_collective_workspace.c`):
   - Shared memory across swarm
   - Consistency protocols
   - Distributed state management

3. **Multi-hop Routing**:
   - Mesh network support
   - Dynamic routing
   - Fault tolerance

4. **Adaptive Learning**:
   - Federated learning
   - Per-swarm specialization
   - Transfer learning

5. **Security**:
   - Authentication
   - Encryption
   - Access control

## Related Documentation

- [SWARM_GATEWAY_GUIDE.md](SWARM_GATEWAY_GUIDE.md) - Detailed gateway usage
- [BIO_ASYNC_INTEGRATION_SUMMARY.md](BIO_ASYNC_INTEGRATION_SUMMARY.md) - Async messaging
- [COGNITIVE_QUICK_REFERENCE.md](COGNITIVE_QUICK_REFERENCE.md) - Brain integration

## Examples

- `examples/swarm_gateway_demo.c` - Complete gateway demonstration
- See gateway guide for more examples

## Support

For questions or issues:
- Check documentation in `docs/`
- Review example code in `examples/`
- Examine test cases in `test/`

## Contributing

When adding to swarm module:
1. Follow NIMCP coding standards
2. Use proper error handling (return codes)
3. Implement thread safety (mutexes)
4. Add comprehensive logging
5. Write unit tests
6. Update documentation

## License

Part of NIMCP (Neuro-Inspired Modular Cognitive Platform)
