# Bio-Async Router - Immune System Integration

## Overview

The Bio-Async Router-Immune Bridge provides bidirectional integration between the bio-async messaging router and the brain immune system. This integration models biological reality where:

1. **Immune signals use neural communication pathways**
2. **Inflammation affects signal routing efficiency**
3. **Immune messaging has priority during threats**
4. **Routing anomalies trigger immune responses**

## Biological Basis

### Immune Signals Use Neural Pathways

**Scientific Evidence:**
- Pro-inflammatory cytokines (IL-1β, IL-6, TNF-α) travel along nerve pathways
- Vagus nerve provides fast brain-body immune communication (milliseconds vs hours)
- Cholinergic anti-inflammatory pathway: acetylcholine suppresses cytokine release
- Neural-immune communication uses same neurotransmitter systems as neural signals

**References:**
- Tracey (2009) "Reflex control of immunity"
- Pavlov & Tracey (2012) "Neural circuitry and immunity"
- Dantzer & Kelley (2007) "Cytokines and sickness behavior"

**NIMCP Implementation:**
- Cytokines broadcast on bio-async NOREPINEPHRINE channel (high priority)
- Immune alerts bypass normal routing queues
- Priority levels: TNF-α (10) > IL-1β/IL-6 (8) > IFN-γ (5) > IL-10 (5)

### Inflammation Affects Routing Efficiency

**Scientific Evidence:**
- Neuroinflammation slows neural signal propagation
- Edema (swelling) increases signal latency
- Increased cytokine traffic saturates communication channels
- Inflamed regions exhibit "congestion" similar to network traffic

**References:**
- DiSabato et al. (2016) "Neuroinflammation: the devil is in the details"

**NIMCP Implementation:**

| Inflammation Level | Latency Multiplier | Effect |
|--------------------|-------------------|--------|
| NONE               | 1.0x              | Normal routing |
| LOCAL              | 1.2x              | +20% latency |
| REGIONAL           | 1.5x              | +50% latency |
| SYSTEMIC           | 2.0x              | +100% latency |
| STORM              | 3.0x              | +200% latency |

### Quarantined Nodes Excluded from Routing

**Scientific Evidence:**
- Infected/damaged neurons are isolated from network
- Byzantine (malfunctioning) nodes must not propagate incorrect signals
- Immune system isolates compromised tissue to prevent spread

**NIMCP Implementation:**
- Killer T cell quarantines → node removed from all routing tables
- Option 1: Full isolation (node completely excluded)
- Option 2: High routing cost penalty (discourage but allow emergency routing)
- Quarantine duration tracks with immune system quarantine time
- Trust score-based restoration: higher trust = faster restoration

### Routing Anomalies Trigger Immune Response

**Scientific Evidence:**
- Network failures may indicate attack or node compromise
- Persistent routing failures are treated as threats requiring immune investigation
- Byzantine behavior (inconsistent forwarding, message tampering) triggers defensive response

**NIMCP Implementation:**

| Anomaly Type | Threshold | Severity Contribution |
|--------------|-----------|----------------------|
| Latency spike | >100ms | 3.0 × (observed/threshold) |
| High drop rate | >10% | 4.0 × (observed/threshold) |
| High error rate | >5% | 3.0 × (observed/threshold) |
| Byzantine behavior | Always | 8.0 (fixed) |

## Architecture

```
╔═══════════════════════════════════════════════════════════════════════════╗
║                  BIO-ROUTER IMMUNE BRIDGE                                  ║
╠═══════════════════════════════════════════════════════════════════════════╣
║                                                                            ║
║   ┌────────────────────────────────────────────────────────────────────┐  ║
║   │                  IMMUNE → ROUTER PATHWAYS                           │  ║
║   │                                                                     │  ║
║   │   1. Cytokine Priority Routing                                     │  ║
║   │      - Map cytokine type to priority level (0-10)                  │  ║
║   │      - Use NOREPINEPHRINE channel for immune messages              │  ║
║   │      - Bypass normal routing queues during inflammation            │  ║
║   │                                                                     │  ║
║   │   2. Inflammation Latency Impact                                   │  ║
║   │      - Apply latency multiplier based on inflammation level        │  ║
║   │      - Increase routing cost for inflamed regions                  │  ║
║   │      - Prefer alternative routes around inflamed areas             │  ║
║   │                                                                     │  ║
║   │   3. Quarantine Enforcement                                        │  ║
║   │      - Remove quarantined nodes from routing tables                │  ║
║   │      - Track quarantine duration and trust scores                  │  ║
║   │      - Auto-restore on quarantine expiry or trust recovery         │  ║
║   └────────────────────────────────────────────────────────────────────┘  ║
║                                                                            ║
║   ┌────────────────────────────────────────────────────────────────────┐  ║
║   │                  ROUTER → IMMUNE PATHWAYS                           │  ║
║   │                                                                     │  ║
║   │   1. Routing Anomaly Detection                                     │  ║
║   │      - Monitor latency, drop rate, error rate                      │  ║
║   │      - Compare against thresholds                                  │  ║
║   │      - Compute anomaly severity (1-10)                             │  ║
║   │                                                                     │  ║
║   │   2. Antigen Presentation                                          │  ║
║   │      - Convert routing anomalies to immune antigens                │  ║
║   │      - Encode anomaly characteristics in epitope signature         │  ║
║   │      - Present to brain immune system for response                 │  ║
║   │                                                                     │  ║
║   │   3. Byzantine Behavior Detection                                  │  ║
║   │      - Detect message corruption, inconsistent forwarding          │  ║
║   │      - Extract behavior signature                                  │  ║
║   │      - Present as high-severity antigen (severity 9)               │  ║
║   └────────────────────────────────────────────────────────────────────┘  ║
║                                                                            ║
╚═══════════════════════════════════════════════════════════════════════════╝
```

## API Usage

### Initialization

```c
#include "async/immune/nimcp_bio_router_immune_bridge.h"

/* Initialize bio-router */
bio_router_config_t router_config = bio_router_default_config();
bio_router_init(&router_config);
bio_router_t router = bio_router_get_global();

/* Create brain immune system */
brain_immune_config_t immune_config;
brain_immune_default_config(&immune_config);
brain_immune_system_t* immune = brain_immune_create(&immune_config);

/* Create router-immune bridge */
router_immune_config_t bridge_config;
router_immune_default_config(&bridge_config);

router_immune_bridge_t* bridge = router_immune_bridge_create(
    &bridge_config,
    router,
    immune
);

/* Start integration */
router_immune_bridge_start(bridge);
```

### Immune → Router: Cytokine Priority Routing

```c
/* Immune system releases TNF-α cytokine */
brain_immune_release_cytokine(
    immune,
    BRAIN_CYTOKINE_TNF,
    source_cell_id,
    0.9f,  /* high concentration */
    0,     /* broadcast */
    &cytokine_id
);

/* Bridge automatically prioritizes TNF-α messages */
router_immune_prioritize_cytokine(
    bridge,
    BRAIN_CYTOKINE_TNF,
    0.9f,
    source_cell_id
);

/* Result: TNF-α messages get priority level 10 (CRITICAL) */
/* Routed on NOREPINEPHRINE channel, bypass normal queues */
```

### Immune → Router: Inflammation Latency Impact

```c
/* Immune system escalates to regional inflammation */
brain_immune_escalate_inflammation(immune, site_id);

/* Bridge applies latency impact to affected region */
router_immune_apply_inflammation_latency(
    bridge,
    region_id,
    INFLAMMATION_REGIONAL
);

/* Result: Routing latency increased by 50% for this region */
float multiplier = router_immune_get_latency_multiplier(bridge, region_id);
/* multiplier = 1.5 */
```

### Immune → Router: Node Quarantine

```c
/* Killer T cell quarantines Byzantine node */
brain_immune_t_cell_kill(immune, t_cell_id, target_node);

/* Bridge excludes node from routing */
router_immune_quarantine_node(
    bridge,
    target_node,
    60000,  /* 60 second quarantine */
    0.2f,   /* low trust score */
    antigen_id
);

/* Result: Node removed from all routing tables */
bool quarantined = router_immune_is_node_quarantined(bridge, target_node);
/* quarantined = true */
```

### Router → Immune: Anomaly Detection

```c
/* Update routing statistics */
router_immune_update_stats(bridge);

/* Check for anomalies on specific node */
router_immune_detect_anomalies(bridge, node_id);

/* If anomaly detected, automatically presented as antigen */
/* Immune system investigates and may trigger response */
```

### Router → Immune: Byzantine Behavior

```c
/* Detect Byzantine routing behavior */
uint8_t behavior_signature[32];
/* ... extract signature from corrupted message ... */

/* Present to immune system */
router_immune_present_byzantine(
    bridge,
    byzantine_node_id,
    behavior_signature,
    sizeof(behavior_signature)
);

/* Result: Antigen created with severity 9 */
/* Immune system activates killer T cells for quarantine */
```

### Bidirectional Update Loop

```c
/* Main loop integration */
while (running) {
    /* Update bridge (both directions) */
    router_immune_bridge_update(bridge, delta_ms);

    /* Bridge automatically:
     * - Applies cytokine routing priorities
     * - Updates inflammation latency impacts
     * - Detects routing anomalies
     * - Enforces quarantines
     * - Expires old cytokine states
     * - Releases expired quarantines
     */

    /* Other system updates */
    brain_immune_update(immune, delta_ms);
}
```

## Configuration Options

### Default Configuration

```c
router_immune_config_t config;
router_immune_default_config(&config);

/* Feature flags (all enabled by default) */
config.enable_cytokine_priority_routing = true;
config.enable_inflammation_latency_impact = true;
config.enable_quarantine_routing_exclusion = true;
config.enable_anomaly_immune_trigger = true;
config.enable_byzantine_detection = true;

/* Capacity limits */
config.max_cytokine_states = 128;
config.max_inflammation_sites = 64;
config.max_quarantined_nodes = 256;
config.max_anomaly_history = 512;

/* Detection thresholds */
config.latency_spike_threshold_ms = 100.0f;
config.drop_rate_threshold = 0.10f;  /* 10% */
config.error_rate_threshold = 0.05f;  /* 5% */

/* Routing behavior */
config.fully_isolate_quarantined = true;
config.cytokine_ttl_ms = 5000;  /* 5 seconds */
```

### Custom Configuration Examples

#### High Sensitivity (Detect More Anomalies)

```c
router_immune_config_t config;
router_immune_default_config(&config);

/* Lower thresholds for earlier detection */
config.latency_spike_threshold_ms = 50.0f;  /* 50ms instead of 100ms */
config.drop_rate_threshold = 0.05f;  /* 5% instead of 10% */
config.error_rate_threshold = 0.02f;  /* 2% instead of 5% */

/* More history tracking */
config.max_anomaly_history = 1024;
```

#### Low Overhead (Minimal Monitoring)

```c
router_immune_config_t config;
router_immune_default_config(&config);

/* Disable expensive features */
config.enable_anomaly_immune_trigger = false;
config.enable_byzantine_detection = false;

/* Smaller capacities */
config.max_cytokine_states = 32;
config.max_anomaly_history = 128;
```

#### Partial Quarantine (High Cost vs Full Isolation)

```c
router_immune_config_t config;
router_immune_default_config(&config);

/* Don't fully isolate quarantined nodes */
/* Instead, apply high routing cost penalty */
config.fully_isolate_quarantined = false;

/* Nodes can still route in emergencies, but discouraged */
```

## Query API

### Get Routing Statistics

```c
router_immune_stats_t stats;
router_immune_get_stats(bridge, &stats);

printf("Messages sent: %lu\n", stats.messages_sent);
printf("Drop rate: %.2f%%\n", stats.current_drop_rate * 100.0f);
printf("Avg latency: %.2f ms\n", stats.avg_latency_ms);
```

### Check Quarantine Status

```c
bool is_quarantined = router_immune_is_node_quarantined(bridge, node_id);
if (is_quarantined) {
    printf("Node %u is quarantined\n", node_id);
}
```

### Get Latency Multiplier

```c
float multiplier = router_immune_get_latency_multiplier(bridge, region_id);
printf("Region %u latency multiplier: %.2fx\n", region_id, multiplier);
```

### Get Cytokine Priority

```c
uint32_t priority = router_immune_get_cytokine_priority(
    bridge, BRAIN_CYTOKINE_TNF
);
printf("TNF-α priority: %u\n", priority);
```

### Get Anomaly Count

```c
/* Get all anomalies */
uint32_t total = router_immune_get_anomaly_count(bridge, 0);

/* Get anomalies in last 60 seconds */
uint32_t recent = router_immune_get_anomaly_count(bridge, 60000);

printf("Total anomalies: %u, Recent: %u\n", total, recent);
```

## Integration with Other Systems

### Blood-Brain Barrier (BBB) Integration

```c
/* BBB threat detected */
bbb_threat_detected(bbb, threat_type, severity, data, len);

/* Presented as antigen to immune system */
brain_immune_present_bbb_threat(immune, threat_type, severity, data, len, &antigen_id);

/* If severe enough, immune system broadcasts alert */
brain_immune_broadcast_alert(immune, antigen_id, INFLAMMATION_SYSTEMIC);

/* Bridge automatically prioritizes alert message */
/* Routed on NOREPINEPHRINE channel with priority 10 */
```

### Byzantine Fault Tolerance (BFT) Integration

```c
/* BFT detects Byzantine node */
bft_report_byzantine(bft, byzantine_node, behavior, evidence, count);

/* Immune system quarantines node */
brain_immune_handle_bft_accusation(immune, accuser, accused, behavior, evidence, count);
brain_immune_activate_killer_t(immune, antigen_id, &killer_id);
brain_immune_t_cell_kill(immune, killer_id, byzantine_node);

/* Bridge enforces routing quarantine */
router_immune_quarantine_node(bridge, byzantine_node, duration, trust, antigen_id);

/* Byzantine node excluded from all routes */
```

### Swarm Immune Integration

```c
/* Swarm detects distributed threat */
swarm_immune_detect_threat(swarm_immune, threat_pattern, severity);

/* Presented to brain immune */
brain_immune_present_swarm_threat(immune, threat, &antigen_id);

/* Immune system coordinates distributed response */
brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
brain_immune_activate_helper_t(immune, antigen_id, &helper_id);

/* Helper T releases cytokines for coordination */
brain_immune_t_help_b(immune, helper_id, b_cell_id);
brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, helper_id, 0.8f, 0, &cyto_id);

/* Bridge ensures cytokine broadcast reaches all swarm nodes */
router_immune_prioritize_cytokine(bridge, BRAIN_CYTOKINE_IL6, 0.8f, helper_id);
```

## Performance Considerations

### Memory Footprint

```
Base structure:           ~200 bytes
Cytokine states (128):    ~3 KB
Inflammation sites (64):  ~2 KB
Quarantined nodes (256):  ~6 KB
Anomaly history (512):    ~32 KB
──────────────────────────────────
Total:                    ~43 KB
```

### CPU Overhead

- **Cytokine prioritization**: O(1) per cytokine
- **Quarantine check**: O(n) where n = quarantined nodes (optimizable with hash table)
- **Anomaly detection**: O(1) statistics comparison
- **Update cycle**: O(n + m + k) where n=cytokines, m=quarantines, k=inflammation sites
- **Typical update**: <100 μs

### Optimization Strategies

1. **Hash table for quarantine lookups**: O(n) → O(1)
2. **Batch cytokine expiry**: Check every N updates instead of every update
3. **Lazy anomaly detection**: Only check specific nodes on message failure
4. **Ring buffer for anomaly history**: Fixed memory, automatic eviction

## Testing

### Unit Tests

Location: `/home/bbrelin/nimcp/test/unit/async/test_bio_router_immune_integration.cpp`

**Test Coverage:**
- Lifecycle (create, destroy, start, stop)
- Cytokine priority routing (all types, multiple cytokines)
- Inflammation latency impact (all levels, updates)
- Node quarantine (single, multiple, restore, expiry)
- Anomaly detection (latency, drop rate, error rate)
- Byzantine behavior detection and presentation
- Bidirectional updates
- Query API
- Error handling

**Run tests:**
```bash
cd /home/bbrelin/nimcp/build
cmake ..
make test_bio_router_immune_integration
./test/unit/async/test_bio_router_immune_integration --gtest_brief=1
```

### Integration Tests

Recommended integration test scenarios:

1. **BBB-Router-Immune Pipeline**: BBB threat → immune antigen → cytokine broadcast → priority routing
2. **BFT-Router-Immune Pipeline**: Byzantine detection → immune quarantine → routing exclusion
3. **Swarm-Router-Immune Pipeline**: Distributed threat → immune coordination → swarm cytokine broadcast
4. **Anomaly-Immune Pipeline**: Routing failure → anomaly detection → immune investigation → quarantine

## Troubleshooting

### Cytokines not prioritized

**Problem**: Cytokine messages don't have higher priority

**Checks**:
1. Is `enable_cytokine_priority_routing` enabled?
2. Is bridge started (`router_immune_bridge_start`)?
3. Is module registered with router (`module_ctx` not null)?

**Solution**: Ensure bridge is properly initialized and started before releasing cytokines.

### Quarantined nodes still routing

**Problem**: Messages still routed through quarantined nodes

**Checks**:
1. Is `enable_quarantine_routing_exclusion` enabled?
2. Is `fully_isolate_quarantined` set to true?
3. Call `router_immune_is_node_quarantined()` to verify status

**Solution**: Verify quarantine applied and check routing implementation respects exclusion list.

### No anomalies detected

**Problem**: Router issues not triggering immune response

**Checks**:
1. Is `enable_anomaly_immune_trigger` enabled?
2. Are thresholds too high? Lower `latency_spike_threshold_ms`, `drop_rate_threshold`
3. Call `router_immune_update_stats()` to ensure fresh statistics

**Solution**: Lower detection thresholds or manually call `router_immune_detect_anomalies()`.

### High memory usage

**Problem**: Bridge consuming too much memory

**Checks**:
1. Check `max_anomaly_history` - reduce if too large
2. Are old cytokines/quarantines being expired?
3. Call `router_immune_bridge_update()` regularly

**Solution**: Reduce capacity limits, ensure update cycle runs regularly.

## Future Enhancements

### Planned Features

1. **Adaptive Thresholds**: Learn normal routing baseline, detect anomalies based on deviation
2. **Geolocation-aware Routing**: Account for physical node distance in latency calculations
3. **Priority Queues**: Implement true multi-level priority queuing in bio-router
4. **Routing Table Integration**: Direct integration with routing table for quarantine enforcement
5. **Metrics Export**: Export routing-immune metrics to monitoring systems

### Research Directions

1. **Neural-immune routing algorithms**: How should bio-inspired routing differ from IP routing?
2. **Optimal quarantine duration**: How long should nodes be quarantined based on threat severity?
3. **Cytokine flood control**: Prevent cytokine storm from overwhelming router
4. **Trust-based gradual restoration**: Slowly restore routing capacity as trust rebuilds

## References

### Biological Literature

1. Tracey, K. J. (2009). "Reflex control of immunity." *Nature Reviews Immunology*, 9(6), 418-428.
2. Pavlov, V. A., & Tracey, K. J. (2012). "The vagus nerve and the inflammatory reflex—linking immunity and metabolism." *Nature Reviews Endocrinology*, 8(12), 743-754.
3. Dantzer, R., & Kelley, K. W. (2007). "Twenty years of research on cytokine-induced sickness behavior." *Brain, Behavior, and Immunity*, 21(2), 153-160.
4. DiSabato, D. J., Quan, N., & Godbout, J. P. (2016). "Neuroinflammation: the devil is in the details." *Journal of Neurochemistry*, 139, 136-153.

### NIMCP Documentation

- [Brain Immune System](BRAIN_IMMUNE_SYSTEM.md)
- [Bio-Async Router](BIO_ASYNC_ROUTER.md)
- [Byzantine Fault Tolerance](BYZANTINE_FAULT_TOLERANCE.md)
- [Blood-Brain Barrier](BLOOD_BRAIN_BARRIER.md)
- [Swarm Immune System](SWARM_IMMUNE_SYSTEM.md)

## Authors

NIMCP Development Team

## License

See project LICENSE file.
