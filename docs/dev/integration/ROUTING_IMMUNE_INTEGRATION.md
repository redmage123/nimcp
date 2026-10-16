# Routing Immune Integration

## Overview

This document describes the integration of the NIMCP brain immune system with the middleware routing and events modules, implementing bidirectional communication between immune responses and neural signal routing.

## Biological Basis

### Inflammation Effects on Neural Routing

**Biological Inspiration:**
- Inflammatory cytokines (IL-1, IL-6, TNF-α) modulate neural excitability and attention
- Pro-inflammatory states increase vigilance and threat-prioritization
- Anti-inflammatory cytokines (IL-10) promote resolution and calming
- Thalamic gating is modulated by immune signaling molecules

**NIMCP Implementation:**
- Inflammation levels (LOCAL → REGIONAL → SYSTEMIC → STORM) boost routing priorities
- Cytokine types modulate attention gate weights
- Immune phases determine overall routing strategy (NORMAL → ALERT → DEFENSIVE → EMERGENCY)

### Routing Dysfunction as Immune Trigger

**Biological Inspiration:**
- Damaged neurons release danger signals (DAMPs) that activate microglia
- Neural dysfunction triggers protective immune responses
- Routing failures are analogous to synaptic dysfunction

**NIMCP Implementation:**
- High drop rates → ROUTING_ANOMALY_HIGH_DROP_RATE
- High latency → ROUTING_ANOMALY_HIGH_LATENCY
- Queue overflow → ROUTING_ANOMALY_QUEUE_OVERFLOW
- Anomalies presented as antigens to immune system

## Architecture

### Component Integration

```
┌─────────────────────────────────────────────────────────────────┐
│                   ROUTING IMMUNE BRIDGE                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Components Connected:                                           │
│  - brain_immune_system_t (Brain Immune System)                  │
│  - thalamic_router_t (Thalamic Router)                          │
│  - attention_gate_t (Attention Gate)                            │
│  - event_bus_t (Event Bus)                                      │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Bidirectional Communication

#### Immune → Routing

1. **Inflammation Effects:**
   - `routing_immune_apply_inflammation_effect()` - Modulates routing priorities
   - LOCAL: +10% priority boost
   - REGIONAL: +30% priority boost
   - SYSTEMIC: +60% priority boost
   - STORM: +100% priority boost (emergency)

2. **Cytokine Effects:**
   - `routing_immune_apply_cytokine_effect()` - Modulates attention weights
   - Pro-inflammatory (IL-1, IL-6, TNF-α, IFN-γ): +25% attention boost
   - Anti-inflammatory (IL-10): -20% attention calm

3. **Strategy Setting:**
   - `routing_immune_set_strategy_from_phase()` - Sets routing strategy
   - SURVEILLANCE/MEMORY → NORMAL routing
   - RECOGNITION → ALERT routing
   - ACTIVATION → DEFENSIVE routing
   - EFFECTOR/RESOLUTION → EMERGENCY routing

#### Routing → Immune

1. **Anomaly Detection:**
   - `routing_immune_detect_anomaly()` - Monitors routing statistics
   - Drop rate threshold: 5%
   - Latency threshold: 100ms
   - Queue overflow threshold: 90% capacity

2. **Antigen Presentation:**
   - `routing_immune_present_anomaly()` - Converts anomaly to antigen
   - Creates epitope signature from anomaly metrics
   - Presents to brain immune system as ANTIGEN_SOURCE_ANOMALY

3. **Event Publication:**
   - `routing_immune_publish_event()` - Publishes immune events to event bus
   - Enables other modules to respond to immune activity

## API Reference

### Lifecycle

```c
// Create routing immune bridge
routing_immune_config_t config = routing_immune_default_config();
routing_immune_bridge_t* bridge = routing_immune_create(
    immune_system, router, attention_gate, event_bus, &config
);

// Destroy bridge
routing_immune_destroy(bridge);
```

### Immune → Routing

```c
// Apply inflammation effect
routing_immune_apply_inflammation_effect(bridge, INFLAMMATION_SYSTEMIC);

// Apply cytokine effect
routing_immune_apply_cytokine_effect(bridge, CYTOKINE_IL1, 0.7f);

// Set routing strategy
routing_immune_set_strategy_from_phase(bridge, IMMUNE_PHASE_EFFECTOR);

// Query current state
float boost = routing_immune_get_inflammation_boost(bridge);
float modifier = routing_immune_get_cytokine_modifier(bridge);
routing_immune_strategy_t strategy = routing_immune_get_strategy(bridge);
```

### Routing → Immune

```c
// Detect anomaly from routing statistics
routing_stats_t stats;
thalamic_router_get_stats(router, &stats);

bool detected;
routing_anomaly_type_t type;
routing_immune_detect_anomaly(bridge, &stats, &detected, &type);

// Present anomaly to immune system
if (detected) {
    routing_anomaly_t anomaly = {
        .type = type,
        .severity = 0.8f,
        // ... other fields
    };

    uint32_t antigen_id;
    routing_immune_present_anomaly(bridge, &anomaly, &antigen_id);
}

// Record anomaly for tracking
routing_immune_record_anomaly(bridge, &anomaly);
```

### Update and Query

```c
// Periodic update (call in main loop)
routing_immune_update(bridge, delta_ms);

// Get statistics
routing_immune_stats_t stats;
routing_immune_get_stats(bridge, &stats);
```

## Event Types Added

### New Event Types (nimcp_event_types.h)

```c
// Immune events
EVENT_TYPE_IMMUNE_ANTIGEN_DETECTED
EVENT_TYPE_IMMUNE_RESPONSE_ACTIVATED
EVENT_TYPE_IMMUNE_THREAT_NEUTRALIZED
EVENT_TYPE_IMMUNE_INFLAMMATION
EVENT_TYPE_IMMUNE_CYTOKINE_RELEASED
EVENT_TYPE_IMMUNE_MEMORY_FORMED

// Routing immune events
EVENT_TYPE_ROUTING_ANOMALY
EVENT_TYPE_ROUTING_IMMUNE_MODULATION
```

### New Event Sources

```c
EVENT_SOURCE_IMMUNE
EVENT_SOURCE_ROUTING_IMMUNE
```

### Event Data Structures

```c
immune_antigen_data_t        // Antigen detection events
immune_response_data_t       // Response activation events
immune_neutralize_data_t     // Threat neutralization events
immune_inflammation_data_t   // Inflammation state changes
immune_cytokine_data_t       // Cytokine signaling events
routing_anomaly_data_t       // Routing anomaly events
routing_immune_modulation_data_t  // Immune modulation events
```

## Configuration

### Default Configuration

```c
routing_immune_config_t config = {
    .drop_rate_threshold = 0.05f,           // 5% drop rate
    .latency_threshold_ms = 100.0f,         // 100ms latency
    .queue_overflow_threshold = 900,        // 90% capacity
    .attention_collapse_threshold = 0.1f,   // 10% attention minimum

    .local_inflammation_boost = 0.1f,       // 10% boost
    .regional_inflammation_boost = 0.3f,    // 30% boost
    .systemic_inflammation_boost = 0.6f,    // 60% boost
    .storm_inflammation_boost = 1.0f,       // 100% boost

    .pro_cytokine_attention_boost = 0.25f,  // 25% attention increase
    .anti_cytokine_attention_calm = 0.2f,   // 20% attention decrease

    .enable_immune_events = true,
    .enable_anomaly_detection = true,
    .update_interval_ms = 100               // Check every 100ms
};
```

## Testing

### Unit Tests

**File:** `/home/bbrelin/nimcp/test/unit/middleware/immune/test_routing_immune_integration.cpp`

**Test Coverage:**
- Lifecycle (create/destroy, null handling)
- Inflammation effects (all levels, escalation)
- Cytokine effects (pro/anti-inflammatory, concentration scaling)
- Strategy setting (all immune phases)
- Anomaly detection (drop rate, latency, queue overflow)
- Anomaly presentation to immune system
- Update cycles and statistics
- Error handling (null parameters, invalid inputs)
- Utility functions (type names, string conversions)

**Running Tests:**
```bash
cd /home/bbrelin/nimcp/build
make unit_middleware_immune_routing_immune_integration
./test/unit/middleware/immune/unit_middleware_immune_routing_immune_integration
```

## Files Created/Modified

### New Files

1. **Header:**
   - `/home/bbrelin/nimcp/include/middleware/immune/nimcp_routing_immune.h`

2. **Implementation:**
   - `/home/bbrelin/nimcp/src/middleware/immune/nimcp_routing_immune.c`

3. **Tests:**
   - `/home/bbrelin/nimcp/test/unit/middleware/immune/test_routing_immune_integration.cpp`
   - `/home/bbrelin/nimcp/test/unit/middleware/immune/CMakeLists.txt`

### Modified Files

1. **Event Types:**
   - `/home/bbrelin/nimcp/include/middleware/events/nimcp_event_types.h`
   - Added immune event types and data structures

2. **Build Configuration:**
   - `/home/bbrelin/nimcp/src/lib/CMakeLists.txt`
   - Added routing immune source file

3. **Test Configuration:**
   - `/home/bbrelin/nimcp/test/unit/middleware/CMakeLists.txt`
   - Added immune subdirectory

## Integration Example

```c
// Create components
brain_immune_system_t* immune = brain_immune_create(&immune_config);
thalamic_router_t* router = thalamic_router_create(&router_config);
attention_gate_t* gate = attention_gate_create(&gate_config);
event_bus_t bus = event_bus_create(&bus_config);

// Create routing immune bridge
routing_immune_bridge_t* bridge = routing_immune_create(
    immune, router, gate, bus, NULL
);

// Start immune system
brain_immune_start(immune);

// Main loop
while (running) {
    // Update routing immune bridge
    routing_immune_update(bridge, delta_ms);

    // Inflammation affects routing automatically
    brain_immune_phase_t phase = brain_immune_get_phase(immune);
    routing_immune_set_strategy_from_phase(bridge, phase);

    // Process routing signals (with immune modulation applied)
    thalamic_router_process_queue(router, max_signals, &processed);

    // Check for anomalies
    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);

    bool detected;
    routing_anomaly_type_t type;
    if (routing_immune_detect_anomaly(bridge, &stats, &detected, &type)) {
        if (detected) {
            // Anomaly triggers immune response
            // (handled automatically in routing_immune_update)
        }
    }
}

// Cleanup
routing_immune_destroy(bridge);
event_bus_destroy(bus);
attention_gate_destroy(gate);
thalamic_router_destroy(router);
brain_immune_destroy(immune);
```

## Performance Considerations

1. **Update Frequency:**
   - Default: 100ms update interval
   - Configurable via `update_interval_ms`
   - Balance between responsiveness and overhead

2. **Anomaly Tracking:**
   - Limited to 64 anomalies (ROUTING_IMMUNE_MAX_ANOMALIES)
   - Oldest anomalies replaced when capacity reached
   - Prevents unbounded memory growth

3. **Thread Safety:**
   - All operations are mutex-protected
   - Safe for concurrent access from immune and routing threads

## Future Enhancements

1. **Learning Mechanisms:**
   - Adaptive threshold tuning based on false positive/negative rates
   - Pattern recognition for recurring anomaly types

2. **Multi-Level Routing:**
   - Per-route inflammation tracking
   - Region-specific immune modulation

3. **Event Bus Integration:**
   - Full callback implementation for routing events
   - Async event processing for immune responses

4. **Metrics and Monitoring:**
   - Enhanced statistics (latency distributions, anomaly patterns)
   - Performance profiling hooks

## References

- Brain Immune System: `/home/bbrelin/nimcp/include/cognitive/immune/nimcp_brain_immune.h`
- Thalamic Router: `/home/bbrelin/nimcp/include/middleware/routing/nimcp_thalamic_router.h`
- Event Types: `/home/bbrelin/nimcp/include/middleware/events/nimcp_event_types.h`
- NIMCP Documentation: `/home/bbrelin/nimcp/CLAUDE.md`

## Author

NIMCP Development Team
Date: 2025-12-11
Version: 1.0.0
