# Phase 1.5: Cognitive-Middleware Integration Plan

**Date**: November 21, 2025
**Status**: Planning
**Priority**: HIGH - Unifies low-level and high-level cognitive processing
**Impact**: Enables true cognitive architecture with bidirectional information flow

## Executive Summary

**Goal**: Connect middleware layer (signal processing, pattern detection, temporal buffering) with cognitive layer (Executive, Global Workspace, Introspection) to create a unified cognitive architecture.

**Current Problem**: Cognitive modules and middleware operate in isolation:
- Middleware processes spike trains → but cognitive modules don't see the patterns
- Cognitive modules make decisions → but can't access signal statistics
- Pattern detection fires → but no one is notified
- Oscillations change → but Executive doesn't adjust task strategy

**Solution**: Event-driven integration with bidirectional data flow and cognitive context propagation.

## Architecture Overview

### Current State (Disconnected)

```
┌──────────────────────────────────────────────────────────┐
│  COGNITIVE LAYER                                         │
│   ├─ Executive Controller (task management)              │
│   ├─ Global Workspace (conscious broadcast)              │
│   ├─ Introspection (self-awareness)                      │
│   └─ Connected via Global Workspace broadcasts           │
└─────────────────────────────────────────────────────────┘
                       ⇕ NO CONNECTION
┌──────────────────────────────────────────────────────────┐
│  MIDDLEWARE LAYER (Phase 1 work)                         │
│   ├─ Temporal Buffers (stats, integration, accumulation) │
│   ├─ Signal Routing (thalamic router, attention gates)   │
│   ├─ Pattern Detection (oscillations, synchrony, seq)    │
│   ├─ Feature Extraction (rate, ISI, spike counts)        │
│   └─ Event System (EVENT_TYPE_PATTERN_DETECTED, etc.)    │
└──────────────────────────────────────────────────────────┘
                       ⇕
┌──────────────────────────────────────────────────────────┐
│  NEURAL NETWORK LAYER                                     │
│   └─ Adaptive network, spike trains, plasticity          │
└──────────────────────────────────────────────────────────┘
```

### Target State (Integrated)

```
┌──────────────────────────────────────────────────────────┐
│  COGNITIVE LAYER                                         │
│   ├─ Executive Controller                                │
│   │   ├─ Receives: Pattern events, oscillation changes   │
│   │   ├─ Sends: Task context, inhibition commands        │
│   │   └─ Uses: Temporal stats for task switching         │
│   │                                                       │
│   ├─ Global Workspace                                    │
│   │   ├─ Receives: Salience peaks, pattern matches       │
│   │   ├─ Broadcasts: Conscious content to middleware     │
│   │   └─ Uses: Synchrony for broadcast decisions         │
│   │                                                       │
│   └─ Introspection                                       │
│       ├─ Receives: Signal stats, uncertainty estimates   │
│       ├─ Queries: Integration buffers, feature vectors   │
│       └─ Reports: Internal state to executive/GW         │
└──────────────────────────────────────────────────────────┘
                       ⇕ EVENT BUS (bidirectional)
┌──────────────────────────────────────────────────────────┐
│  EVENT BUS & INTEGRATION LAYER                           │
│   ├─ Cognitive Event Adapter                             │
│   │   └─ Translates middleware events → cognitive msgs   │
│   │                                                       │
│   ├─ Middleware Command Adapter                          │
│   │   └─ Translates cognitive commands → middleware ops  │
│   │                                                       │
│   └─ Data Access Layer                                   │
│       └─ Provides temporal buffer/stats API to cognitive │
└──────────────────────────────────────────────────────────┘
                       ⇕
┌──────────────────────────────────────────────────────────┐
│  MIDDLEWARE LAYER                                        │
│   ├─ Event Publishers (detect → publish)                 │
│   ├─ Command Handlers (receive cognitive commands)       │
│   └─ Data Providers (expose buffer stats)                │
└──────────────────────────────────────────────────────────┘
```

## Integration Points

### 1. Middleware → Cognitive (Upward Flow)

Events from middleware inform cognitive processes:

| Middleware Event | Cognitive Module | Purpose |
|------------------|------------------|---------|
| `EVENT_TYPE_PATTERN_DETECTED` | Executive | Task switching based on learned patterns |
| `EVENT_TYPE_OSCILLATION_CHANGE` | Executive | Mode switching (theta → memory encoding) |
| `EVENT_TYPE_SPIKE_BURST` | Introspection | Self-awareness of neural activity |
| `EVENT_TYPE_SALIENCE_PEAK` | Global Workspace | Trigger workspace competition |
| `EVENT_TYPE_SYNCHRONY_HIGH` | Global Workspace | Indication for broadcast ignition |
| `EVENT_TYPE_NOVELTY_DETECTED` | Executive | Attention reorientation |
| `EVENT_TYPE_ERROR_DETECTED` | Introspection | Uncertainty assessment |

### 2. Cognitive → Middleware (Downward Flow)

Cognitive modules configure middleware behavior:

| Cognitive Module | Middleware Target | Command |
|------------------|-------------------|---------|
| Executive | Attention Gates | Adjust salience thresholds |
| Executive | Signal Router | Configure routing tables |
| Global Workspace | Pattern Detector | Subscribe to specific patterns |
| Introspection | Temporal Buffer | Query statistics on demand |
| Executive | Feature Extractor | Change extraction parameters |

### 3. Data Access (Query Interface)

Cognitive modules query middleware state:

| Cognitive Module | Data Accessed | Use Case |
|------------------|---------------|----------|
| Introspection | Integration buffer stats | Assess internal signal quality |
| Introspection | Sliding window statistics | Measure temporal stability |
| Executive | Feature extraction history | Task performance metrics |
| Global Workspace | Pattern library state | Identify conscious-worthy patterns |
| Introspection | Oscillation band power | Brain state awareness |

## Implementation Phases

### Phase 1.5.1: Event Bus Integration (Core Infrastructure)

**Goal**: Establish event routing between middleware and cognitive layers

**Files to Create**:
- `include/middleware/integration/nimcp_cognitive_adapter.h`
- `src/middleware/integration/nimcp_cognitive_adapter.c`
- `include/middleware/integration/nimcp_event_router.h`
- `src/middleware/integration/nimcp_event_router.c`

**Key Components**:

```c
/**
 * @brief Cognitive event adapter - bridges middleware events to cognitive modules
 */
typedef struct {
    // Event subscriptions
    executive_controller_t* executive;
    global_workspace_t* global_workspace;
    introspection_context_t introspection;

    // Event filters
    bool enable_pattern_events;
    bool enable_oscillation_events;
    bool enable_salience_events;

    // Statistics
    uint64_t events_routed;
    uint64_t events_dropped;
} cognitive_event_adapter_t;

// Subscribe cognitive modules to middleware events
cognitive_event_adapter_t* cognitive_adapter_create(
    executive_controller_t* executive,
    global_workspace_t* workspace,
    introspection_context_t introspection
);

// Route middleware event to appropriate cognitive module
void cognitive_adapter_route_event(
    cognitive_event_adapter_t* adapter,
    const middleware_event_t* event
);
```

**Expected Results**:
- Events flow from middleware → cognitive modules
- Zero-copy event routing (pointer passing, no serialization)
- Configurable filtering (only send relevant events)
- Performance: <10µs routing overhead per event

---

### Phase 1.5.2: Executive-Middleware Integration

**Goal**: Enable Executive Controller to respond to neural patterns and control middleware

**Files to Modify**:
- `src/include/cognitive/nimcp_executive.h` (add event handlers)
- `src/cognitive/executive/nimcp_executive.c` (implement handlers)
- `src/middleware/patterns/nimcp_pattern_library.c` (add notification)

**New APIs**:

```c
// Executive receives pattern detection events
void executive_on_pattern_detected(
    executive_controller_t* exec,
    const pattern_detected_data_t* pattern
);

// Executive receives oscillation state changes
void executive_on_oscillation_change(
    executive_controller_t* exec,
    const oscillation_change_data_t* oscillation
);

// Executive configures attention gates based on task
void executive_set_attention_priority(
    executive_controller_t* exec,
    attention_gate_t* gate,
    task_type_t task_type,
    float priority
);
```

**Integration Logic**:

```c
// Example: Task switching based on theta oscillations
void executive_on_oscillation_change(
    executive_controller_t* exec,
    const oscillation_change_data_t* osc
) {
    if (osc->band == THETA_BAND && osc->power > 0.7) {
        // High theta → memory encoding mode
        executive_switch_task_mode(exec, TASK_MODE_MEMORY_ENCODING);

        // Adjust attention to favor hippocampal signals
        executive_set_attention_priority(exec,
            exec->hippocampal_gate,
            TASK_TYPE_MEMORY_RETRIEVAL,
            0.9);
    }
}
```

**Expected Results**:
- Executive adapts to brain state (oscillations guide task modes)
- Task switching informed by pattern detection
- Attention gates controlled by task requirements
- Performance: <50µs processing per event

---

### Phase 1.5.3: Global Workspace-Middleware Integration

**Goal**: Use middleware events to trigger workspace competition and broadcasts

**Files to Modify**:
- `src/cognitive/global_workspace/nimcp_global_workspace.c`
- `src/middleware/patterns/nimcp_pattern_library.c`
- `src/middleware/features/nimcp_feature_extractor.c`

**New APIs**:

```c
// Global workspace receives salience peak events
bool global_workspace_on_salience_peak(
    global_workspace_t* workspace,
    const salience_peak_data_t* peak
);

// Pattern library queries workspace for conscious patterns
bool pattern_should_broadcast(
    global_workspace_t* workspace,
    uint32_t pattern_id
);

// Workspace broadcasts to middleware subscribers
void global_workspace_broadcast_to_middleware(
    global_workspace_t* workspace,
    const float* content,
    uint32_t dim
);
```

**Integration Logic**:

```c
// Example: Salience peak triggers workspace competition
bool global_workspace_on_salience_peak(
    global_workspace_t* workspace,
    const salience_peak_data_t* peak
) {
    // Convert salience data to workspace content
    float content[GLOBAL_WORKSPACE_DEFAULT_DIM];
    feature_vector_to_workspace_content(
        peak->feature_vector,
        peak->dimension,
        content,
        GLOBAL_WORKSPACE_DEFAULT_DIM
    );

    // Compete for workspace with salience as strength
    return global_workspace_compete(
        workspace,
        MODULE_SALIENCE,
        content,
        GLOBAL_WORKSPACE_DEFAULT_DIM,
        peak->salience_score  // Use salience as competition strength
    );
}
```

**Expected Results**:
- Salient patterns automatically compete for consciousness
- Workspace broadcasts inform middleware processing
- Pattern detection biased toward conscious content
- Performance: <100µs for workspace competition

---

### Phase 1.5.4: Introspection-Middleware Integration

**Goal**: Give Introspection access to middleware statistics for self-assessment

**Files to Modify**:
- `src/cognitive/introspection/nimcp_introspection.c`
- `src/middleware/buffering/nimcp_integration_buffer.c`
- `src/middleware/buffering/nimcp_sliding_window.c`

**New APIs**:

```c
// Introspection queries temporal buffer statistics
brain_signal_quality_t introspection_assess_signal_quality(
    introspection_context_t introspection,
    brain_temporal_buffer_t* temporal_buffer
);

// Introspection monitors oscillation coherence
float introspection_get_brain_coherence(
    introspection_context_t introspection,
    oscillation_detector_t* detector
);

// Introspection receives error/uncertainty events
void introspection_on_prediction_error(
    introspection_context_t introspection,
    const error_detected_data_t* error
);
```

**Integration Logic**:

```c
// Example: Assess signal quality from temporal buffers
brain_signal_quality_t introspection_assess_signal_quality(
    introspection_context_t introspection,
    brain_temporal_buffer_t* temporal_buffer
) {
    // Query sliding window statistics
    window_stats_t stats = sliding_window_get_stats(temporal_buffer->window);

    // High variance → low confidence
    float signal_stability = 1.0f - fminf(stats.stddev / stats.mean, 1.0f);

    // Query integration buffer for multi-scale consistency
    float fast_power = integration_buffer_get_power(
        temporal_buffer->multiscale,
        TIMESCALE_FAST
    );
    float slow_power = integration_buffer_get_power(
        temporal_buffer->multiscale,
        TIMESCALE_SLOW
    );

    // Multi-scale agreement → high quality
    float scale_consistency = 1.0f - fabsf(fast_power - slow_power);

    return (brain_signal_quality_t){
        .stability = signal_stability,
        .consistency = scale_consistency,
        .overall_quality = (signal_stability + scale_consistency) / 2.0f
    };
}
```

**Expected Results**:
- Introspection has real-time signal quality awareness
- Self-assessment based on actual neural statistics
- Uncertainty estimates informed by temporal variability
- Performance: <20µs for statistics query

---

### Phase 1.5.5: Command Interface (Cognitive → Middleware)

**Goal**: Allow cognitive modules to configure middleware behavior

**Files to Create**:
- `include/middleware/integration/nimcp_middleware_controller.h`
- `src/middleware/integration/nimcp_middleware_controller.c`

**New APIs**:

```c
/**
 * @brief Middleware controller - receives commands from cognitive modules
 */
typedef struct {
    thalamic_router_t* router;
    attention_gate_t* attention_gates;
    pattern_library_t* patterns;
    feature_extractor_t* features;
} middleware_controller_t;

// Executive adjusts attention thresholds
void middleware_set_attention_threshold(
    middleware_controller_t* controller,
    cognitive_module_t source,
    float threshold
);

// Executive reconfigures routing priorities
void middleware_set_routing_priority(
    middleware_controller_t* controller,
    signal_route_id_t route,
    float priority
);

// Global workspace subscribes to specific patterns
void middleware_subscribe_pattern(
    middleware_controller_t* controller,
    uint32_t pattern_id,
    cognitive_module_t subscriber
);
```

**Integration Logic**:

```c
// Example: Executive adjusts attention based on task
void executive_configure_attention_for_task(
    executive_controller_t* exec,
    middleware_controller_t* middleware,
    task_type_t task
) {
    switch (task) {
        case TASK_TYPE_MEMORY_RETRIEVAL:
            // Boost hippocampal attention for memory tasks
            middleware_set_attention_threshold(
                middleware,
                MODULE_EXECUTIVE,
                0.3  // Lower threshold = more signals pass
            );
            break;

        case TASK_TYPE_REASONING:
            // Boost prefrontal attention for reasoning tasks
            middleware_set_attention_threshold(
                middleware,
                MODULE_EXECUTIVE,
                0.5  // Moderate threshold
            );
            break;
    }
}
```

**Expected Results**:
- Top-down control of attention and routing
- Task-adaptive middleware configuration
- Cognitive context propagates to signal processing
- Performance: <5µs per command

---

## Performance Targets

| Operation | Target Latency | Rationale |
|-----------|----------------|-----------|
| Event routing (middleware → cognitive) | <10µs | Zero-copy pointer passing |
| Executive event handling | <50µs | Simple logic, no complex computation |
| Workspace competition trigger | <100µs | Includes feature transformation |
| Introspection statistics query | <20µs | Direct memory pool access |
| Command dispatch (cognitive → middleware) | <5µs | Function call overhead only |
| **Total overhead per event** | **<200µs** | Negligible compared to neural computation |

## Memory Overhead

| Component | Memory Cost | Justification |
|-----------|-------------|---------------|
| Cognitive adapter | 512 bytes | Event filter flags + pointers |
| Event router | 2 KB | Subscription tables + stats |
| Middleware controller | 1 KB | Configuration state |
| **Total integration overhead** | **<4 KB** | Minimal compared to brain memory |

## Testing Strategy

### Unit Tests

```c
// Test event routing
test_cognitive_adapter_routes_pattern_event()
test_cognitive_adapter_filters_events()
test_executive_responds_to_oscillation()
test_workspace_competes_on_salience_peak()
test_introspection_queries_buffer_stats()

// Test commands
test_executive_sets_attention_threshold()
test_workspace_subscribes_to_pattern()
test_middleware_controller_dispatches_command()
```

### Integration Tests

```c
// End-to-end scenarios
test_pattern_detection_triggers_task_switch()
test_salience_peak_reaches_consciousness()
test_introspection_reports_signal_degradation()
test_executive_reconfigures_attention_gates()
test_workspace_broadcast_affects_pattern_detection()
```

### Performance Benchmarks

```c
// Benchmark event throughput
benchmark_event_routing_latency()          // Target: <10µs
benchmark_executive_event_processing()     // Target: <50µs
benchmark_workspace_competition_trigger()  // Target: <100µs
benchmark_introspection_query_overhead()   // Target: <20µs
benchmark_command_dispatch()               // Target: <5µs

// Benchmark end-to-end scenarios
benchmark_pattern_to_consciousness()       // Target: <500µs
benchmark_task_switch_latency()            // Target: <1ms
```

## Expected Benefits

### 1. Unified Cognitive Architecture
- Bottom-up (data-driven) and top-down (goal-driven) processing unified
- Signals inform cognition, cognition guides signal processing

### 2. Emergent Cognitive Phenomena
- Consciousness emerges from pattern-salience-workspace pipeline
- Metacognition emerges from introspection-statistics feedback
- Adaptive attention emerges from executive-routing control

### 3. Performance Efficiency
- Cognitive modules focus on high-level patterns (not raw spikes)
- Middleware filters irrelevant signals (attention gates)
- Event-driven reduces polling overhead

### 4. Biological Plausibility
- Matches thalamocortical loop architecture
- Executive (DLPFC) controls thalamic gates
- Global workspace (PFC-parietal) integrates signals
- Introspection (anterior cingulate) monitors internal state

### 5. Extensibility
- New cognitive modules can subscribe to events
- New middleware components can publish events
- Minimal coupling via event interface

## Implementation Order

### Week 1: Phase 1.5.1 - Event Bus Infrastructure
**Priority**: HIGHEST (foundation for everything else)
**Dependencies**: None
**Risk**: LOW (new code, isolated)
**LOC**: ~400 lines

### Week 2: Phase 1.5.2 - Executive Integration
**Priority**: HIGH (enables task-adaptive behavior)
**Dependencies**: Phase 1.5.1
**Risk**: MODERATE (modifies executive module)
**LOC**: ~300 lines

### Week 3: Phase 1.5.3 - Global Workspace Integration
**Priority**: HIGH (enables consciousness emergence)
**Dependencies**: Phase 1.5.1
**Risk**: MODERATE (modifies workspace logic)
**LOC**: ~250 lines

### Week 4: Phase 1.5.4 - Introspection Integration
**Priority**: MEDIUM (enhances self-awareness)
**Dependencies**: Phase 1.5.1
**Risk**: LOW (mostly queries, minimal logic)
**LOC**: ~200 lines

### Week 5: Phase 1.5.5 - Command Interface
**Priority**: MEDIUM (enables top-down control)
**Dependencies**: All above
**Risk**: LOW (simple command dispatch)
**LOC**: ~300 lines

**Total Estimated LOC**: ~1450 lines

## Success Criteria

1. **Functional Integration**:
   - ✓ Pattern detection events reach Executive
   - ✓ Salience peaks trigger Global Workspace competition
   - ✓ Introspection queries temporal buffer statistics
   - ✓ Executive commands configure attention gates
   - ✓ Global Workspace broadcasts affect pattern detection

2. **Performance**:
   - ✓ Event routing overhead <10µs per event
   - ✓ Total integration overhead <200µs per event
   - ✓ Memory overhead <4 KB
   - ✓ Zero heap allocations in hot path

3. **Tests**:
   - ✓ 100% unit test coverage for adapters
   - ✓ Integration tests for all event flows
   - ✓ Performance benchmarks meet targets
   - ✓ No memory leaks detected

4. **Documentation**:
   - ✓ Event routing architecture documented
   - ✓ API documentation complete
   - ✓ Usage examples for each integration point
   - ✓ Performance characteristics documented

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Event routing overhead | LOW | MEDIUM | Zero-copy design, benchmark early |
| Cognitive module disruption | MEDIUM | HIGH | Careful API design, incremental integration |
| Event flooding | LOW | MEDIUM | Configurable filters, priority queues |
| Memory fragmentation | LOW | LOW | Pre-allocated buffers, object pools |
| Thread safety issues | MEDIUM | HIGH | Mutex protection, lock-free where possible |

## Questions for Discussion

1. **Event Frequency**: How often should middleware emit events?
   - Option A: Every pattern detection (high frequency, complete info)
   - Option B: Threshold-based (only significant events)
   - Option C: Adaptive rate (based on cognitive load)
   - **Recommendation**: B (threshold-based) initially, then C (adaptive)

2. **Command Latency**: Should cognitive commands be immediate or queued?
   - Option A: Immediate (low latency, potential thread contention)
   - Option B: Queued (consistent latency, slight delay)
   - **Recommendation**: A (immediate) for attention, B (queued) for configuration

3. **Event Persistence**: Should events be logged/stored?
   - Option A: No persistence (minimal overhead)
   - Option B: Ring buffer (recent history for debugging)
   - Option C: Full logging (comprehensive but expensive)
   - **Recommendation**: B (ring buffer, configurable size)

4. **Global Workspace Middleware Access**: Should GW broadcast back to middleware?
   - Option A: Yes (bidirectional, conscious content guides processing)
   - Option B: No (unidirectional, keep layers separate)
   - **Recommendation**: A (yes, for task-relevant attention modulation)

---

**Next Steps**: Review this plan, select implementation order, begin Phase 1.5.1 (Event Bus Infrastructure).
