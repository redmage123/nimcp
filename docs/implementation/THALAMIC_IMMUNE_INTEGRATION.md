# Thalamic Router-Immune Integration

## Overview

Bidirectional integration between the thalamic routing system and brain immune system, modeling biological inflammation effects on sensory gating and threat prioritization.

## Biological Basis

### Immune → Routing Pathways

1. **Cytokine Effects on Thalamic Gating**
   - IL-6 affects thalamic relay neurons (increases excitability)
   - Pro-inflammatory cytokines reduce sensory gating
   - Result: Hypervigilance, reduced filtering during inflammation
   - Reference: Capuron & Miller (2011) "Immune system to brain signaling"

2. **Inflammation-Induced Hypervigilance**
   - Systemic inflammation → increased threat sensitivity
   - Thalamic reticular nucleus (TRN) gating threshold lowered
   - Enhanced detection of threat-related signals
   - Reference: Harrison et al. (2009) "Inflammation and neural response"

3. **Priority Routing During Sickness**
   - Immune-related signals bypass normal filtering
   - Threat detection circuits get enhanced throughput
   - Social signals deprioritized (sickness behavior)
   - Reference: Dantzer et al. (2008) "Sickness behavior mechanisms"

4. **IL-10 Restores Normal Gating**
   - Anti-inflammatory cytokines normalize sensory gating
   - Threshold restoration, reduced hypervigilance
   - Return to balanced attention allocation

### Routing → Immune Pathways

1. **Routing Anomalies as Threats**
   - Aberrant routing patterns suggest neural dysfunction
   - Queue overflow → resource exhaustion threat
   - Excessive signal dropping → system compromise
   - Trigger immune investigation (antigen presentation)

2. **Priority Violations**
   - High-priority signal drops indicate severe threat
   - Repeated violations trigger inflammation
   - Pattern recognition for Byzantine behavior

3. **Routing Health Feedback**
   - Efficient routing → IL-10 release
   - Low latency, high throughput → reduced inflammation
   - Positive feedback loop for system stability

## Architecture

```
╔═══════════════════════════════════════════════════════════════════════════╗
║                    THALAMIC-IMMUNE BRIDGE                                  ║
╠═══════════════════════════════════════════════════════════════════════════╣
║                                                                            ║
║   ┌────────────────────────────────────────────────────────────────────┐  ║
║   │                  IMMUNE → ROUTING PATHWAYS                          │  ║
║   │                                                                     │  ║
║   │   ┌──────────────┐                                                 │  ║
║   │   │  CYTOKINES   │                                                 │  ║
║   │   │ ──────────── │                                                 │  ║
║   │   │ IL-6  → +20% │  ───────┐                                       │  ║
║   │   │ IL-1β → +15% │         │  Increase Routing Priority            │  ║
║   │   │ TNF-α → +25% │         ├─→ (Threat signals, immune-related)   │  ║
║   │   │              │         │                                       │  ║
║   │   └──────────────┘         │                                       │  ║
║   │                            ▼                                       │  ║
║   │   ┌─────────────────────────────────┐                             │  ║
║   │   │     THALAMIC ROUTER             │                             │  ║
║   │   │  - Attention threshold lowered  │                             │  ║
║   │   │  - Threat priority elevated     │                             │  ║
║   │   │  - Social signals deprioritized │                             │  ║
║   │   │  - Sensory gating reduced       │                             │  ║
║   │   └─────────────────────────────────┘                             │  ║
║   │                            ▲                                       │  ║
║   │   ┌──────────────┐         │                                       │  ║
║   │   │   IL-10      │         │                                       │  ║
║   │   │ Anti-inflam  │  ───────┘                                       │  ║
║   │   │   Normalize  │     Restore Normal Gating                       │  ║
║   │   └──────────────┘                                                 │  ║
║   └────────────────────────────────────────────────────────────────────┘  ║
║                                                                            ║
║   ┌────────────────────────────────────────────────────────────────────┐  ║
║   │                  ROUTING → IMMUNE PATHWAYS                          │  ║
║   │                                                                     │  ║
║   │   ┌──────────────┐                                                 │  ║
║   │   │ ANOMALIES    │ ──→ Antigen Presentation                        │  ║
║   │   │ Queue Full   │ ──→ Resource Exhaustion Threat                  │  ║
║   │   │ Signal Drops │ ──→ System Compromise Alert                     │  ║
║   │   │ High Priority│ ──→ Critical Failure → Inflammation             │  ║
║   │   │ Drop         │                                                 │  ║
║   │   └──────────────┘                                                 │  ║
║   │                                                                     │  ║
║   │   ┌──────────────┐                                                 │  ║
║   │   │ ROUTING OK   │ ──→ IL-10 Release (System Healthy)              │  ║
║   │   │ Low Latency  │ ──→ Reduce Inflammation                         │  ║
║   │   └──────────────┘                                                 │  ║
║   └────────────────────────────────────────────────────────────────────┘  ║
║                                                                            ║
╚═══════════════════════════════════════════════════════════════════════════╝
```

## API Summary

### Lifecycle

```c
// Create bridge
thalamic_immune_config_t config;
thalamic_immune_default_config(&config);
thalamic_immune_bridge_t* bridge = thalamic_immune_bridge_create(
    &config,
    immune_system,
    thalamic_router
);

// Update bridge
thalamic_immune_bridge_update(bridge, delta_ms);

// Destroy
thalamic_immune_bridge_destroy(bridge);
```

### Immune → Routing

```c
// Apply cytokine effects to routing priorities
thalamic_immune_apply_cytokine_effects(bridge);

// Apply inflammation to routing behavior (hypervigilance)
thalamic_immune_apply_inflammation_effects(bridge);

// Escalate priority for threat signals
thalamic_immune_escalate_priority(bridge, source_id, dest_id, is_threat);

// Restore normal gating from IL-10
thalamic_immune_restore_gating(bridge);
```

### Routing → Immune

```c
// Detect routing anomalies
thalamic_immune_detect_anomalies(bridge);

// Trigger immune response from anomalies
thalamic_immune_trigger_from_anomaly(bridge);

// Boost immunity from healthy routing
thalamic_immune_boost_from_health(bridge);
```

### Query

```c
// Get cytokine effects
cytokine_routing_effects_t effects;
thalamic_immune_get_cytokine_effects(bridge, &effects);

// Get inflammation state
inflammation_routing_state_t state;
thalamic_immune_get_inflammation_state(bridge, &state);

// Get anomaly state
routing_anomaly_state_t anomaly;
thalamic_immune_get_anomaly_state(bridge, &anomaly);

// Check hypervigilance
bool hypervigilant = thalamic_immune_is_hypervigilant(bridge);

// Get effective gating threshold
float threshold = thalamic_immune_get_gating_threshold(bridge);

// Get threat priority multiplier
float multiplier = thalamic_immune_get_threat_priority_multiplier(bridge);
```

## Key Constants

### Cytokine Modulation
- `CYTOKINE_IL6_PRIORITY_BOOST`: 0.20 (+20% priority from IL-6)
- `CYTOKINE_IL1_PRIORITY_BOOST`: 0.15 (+15% priority from IL-1β)
- `CYTOKINE_TNF_PRIORITY_BOOST`: 0.25 (+25% priority from TNF-α)
- `CYTOKINE_IL10_GATING_RESTORE`: 0.30 (IL-10 restoration)

### Inflammation Effects
- `INFLAMMATION_GATING_REDUCTION`: 0.30 (30% gating reduction)
- `INFLAMMATION_THREAT_PRIORITY`: 1.50 (50% threat priority boost)
- `INFLAMMATION_SOCIAL_DEGRADE`: 0.50 (50% social signal reduction)

### Anomaly Detection
- `ROUTING_ANOMALY_QUEUE_THRESHOLD`: 0.85 (85% queue full)
- `ROUTING_ANOMALY_DROP_THRESHOLD`: 0.10 (10% drop rate)
- `ROUTING_ANOMALY_LATENCY_MS`: 100.0 (100ms latency)

## Integration Points

### With Brain Immune System
- Queries cytokine levels from immune system
- Monitors inflammation level and duration
- Presents routing anomalies as antigens
- Participates in IL-10 release on health

### With Thalamic Router
- Modulates attention weights based on cytokines
- Adjusts gating thresholds based on inflammation
- Escalates priority for threat-related signals
- Monitors routing statistics for anomalies

## Files

- **Header**: `include/middleware/immune/nimcp_thalamic_immune_bridge.h`
- **Implementation**: `src/middleware/immune/nimcp_thalamic_immune_bridge.c`
- **Tests**: `test/unit/middleware/routing/test_thalamic_immune_integration.cpp`

## Test Coverage

The test suite (`test_thalamic_immune_integration.cpp`) includes:

1. **Lifecycle Tests**
   - Bridge creation/destruction
   - Default configuration

2. **Immune → Routing Tests**
   - Cytokine modulation of priority
   - Inflammation-induced hypervigilance
   - Threat signal escalation
   - IL-10 gating restoration

3. **Routing → Immune Tests**
   - Anomaly detection
   - Anomaly-triggered immune response
   - Health feedback

4. **Behavioral Tests**
   - Sickness behavior routing changes
   - Hypervigilance query
   - Gating threshold dynamics
   - Threat priority multiplier

5. **Configuration Tests**
   - Cytokine sensitivity tuning
   - Anomaly threshold configuration
   - Feature toggles

6. **Robustness Tests**
   - Null pointer guards
   - Multiple updates stability
   - Statistics tracking

## Build Instructions

### Add to Library

The module is already added to `src/middleware/immune/CMakeLists.txt`:

```cmake
add_library(nimcp_middleware_immune OBJECT
    nimcp_training_immune.c
    nimcp_thalamic_immune_bridge.c
)
```

### Build Test

```bash
cd /home/bbrelin/nimcp/build
make unit_middleware_routing_thalamic_immune_integration -j4
```

### Run Test

```bash
./test/unit/middleware/routing/unit_middleware_routing_thalamic_immune_integration --gtest_brief=1
```

## Usage Example

```c
// Initialize systems
brain_immune_system_t* immune = brain_immune_create(&immune_config);
thalamic_router_t* router = thalamic_router_create(&router_config);

// Create bridge
thalamic_immune_config_t config;
thalamic_immune_default_config(&config);
thalamic_immune_bridge_t* bridge = thalamic_immune_bridge_create(
    &config,
    immune,
    router
);

// Main loop
while (running) {
    // Update immune system
    brain_immune_update(immune, delta_ms);

    // Update bridge (applies bidirectional effects)
    thalamic_immune_bridge_update(bridge, delta_ms);

    // Route signals through router
    thalamic_router_route_signal(router, &signal);
    thalamic_router_process_queue(router, 10, &processed);

    // Check state
    if (thalamic_immune_is_hypervigilant(bridge)) {
        // System is in hypervigilance mode
        // Threat signals are prioritized
    }
}

// Cleanup
thalamic_immune_bridge_destroy(bridge);
brain_immune_destroy(immune);
thalamic_router_destroy(router);
```

## Biological Implications

### During Inflammation

1. **Sensory Gating Reduced**: Lower threshold means more signals pass through
2. **Threat Focus**: Threat-related signals get priority routing
3. **Social Withdrawal**: Social signals deprioritized (sickness behavior)
4. **Hypervigilance**: Enhanced detection of potential threats

### During Recovery (IL-10)

1. **Gating Restoration**: Threshold returns to normal
2. **Priority Normalization**: Balanced attention allocation
3. **Social Reengagement**: Social signals resume normal priority
4. **Reduced Hypervigilance**: Threat sensitivity normalizes

### Routing Anomalies

1. **Queue Overflow**: Detected as resource exhaustion threat
2. **Signal Drops**: System compromise indicator
3. **Priority Violations**: Critical failure requiring immune response
4. **High Latency**: Performance degradation suggesting dysfunction

## Future Enhancements

1. **Adaptive Thresholds**: Learn optimal thresholds from history
2. **Multi-Region Integration**: Per-region gating adjustments
3. **Temporal Dynamics**: Model gating changes over time
4. **Predictive Anomalies**: Predict routing failures before they occur
5. **Cross-Modal Gating**: Different thresholds for sensory modalities

## References

1. Capuron, L., & Miller, A. H. (2011). "Immune system to brain signaling: Neuropsychopharmacological implications"
2. Harrison, N. A., et al. (2009). "Inflammation causes mood changes through alterations in subgenual cingulate activity and mesolimbic connectivity"
3. Dantzer, R., et al. (2008). "From inflammation to sickness and depression: when the immune system subjugates the brain"
4. Sherman, S. M., & Guillery, R. W. (2001). "Exploring the thalamus and its role in cortical function"

## Author

NIMCP Development Team - December 11, 2025
