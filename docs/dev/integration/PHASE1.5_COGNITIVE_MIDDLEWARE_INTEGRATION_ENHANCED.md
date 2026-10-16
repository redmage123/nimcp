# Phase 1.5: Cognitive-Middleware Integration Plan (Enhanced with Mathematical Theory)

**Date**: November 21, 2025
**Status**: Planning (Enhanced)
**Priority**: HIGHEST - Unifies low-level and high-level cognitive processing with mathematical rigor
**Impact**: Self-optimizing cognitive architecture with information-theoretic guarantees

## Enhancement Summary

This enhanced plan integrates advanced mathematical frameworks:
- ✅ **Shannon Information Theory**: Bottleneck detection, channel capacity optimization
- ✅ **Cross-Modal Flow Tracking**: Monitor information flow between layers
- ✅ **Quantum-Shannon Diffusion**: √N speedup for command propagation
- ✅ **Oscillation Analysis**: Brain state-driven mode switching
- ✅ **Community Detection**: Health monitoring and hub analysis
- ✅ **Quantum Annealing**: Future routing optimization

## Architecture Overview (Enhanced)

### Target State with Mathematical Enhancements

```
┌──────────────────────────────────────────────────────────────┐
│  COGNITIVE LAYER                                             │
│   ├─ Executive Controller                                    │
│   │   ├─ Oscillation-driven mode switching (theta/gamma)     │
│   │   ├─ Shannon-informed task prioritization                │
│   │   └─ Quantum-accelerated command propagation             │
│   │                                                           │
│   ├─ Global Workspace                                        │
│   │   ├─ Information content-based competition               │
│   │   ├─ Channel capacity-aware broadcasting                 │
│   │   └─ Cross-modal flow optimization                       │
│   │                                                           │
│   └─ Introspection                                           │
│       ├─ Signal quality via Shannon metrics                  │
│       ├─ Connectivity health via community detection         │
│       └─ Hub neuron monitoring                               │
└──────────────────────────────────────────────────────────────┘
                ⇕ ENHANCED EVENT BUS (Shannon-monitored)
┌──────────────────────────────────────────────────────────────┐
│  INTEGRATION LAYER (Mathematical Monitoring)                 │
│   ├─ Shannon Monitor                                         │
│   │   ├─ Channel capacity: C = B log₂(1 + SNR)              │
│   │   ├─ Bottleneck detection: H(X|Y) > threshold           │
│   │   └─ Information loss tracking: I(X;Y)                   │
│   │                                                           │
│   ├─ Cross-Modal Flow Tracker                                │
│   │   ├─ Middleware → Cognitive flow rates                   │
│   │   ├─ Cognitive → Middleware command rates                │
│   │   └─ Efficiency metrics: η = I_out / I_in               │
│   │                                                           │
│   ├─ Quantum-Shannon Diffusion Engine                        │
│   │   ├─ O(√N) command propagation                           │
│   │   ├─ Real-time bottleneck detection                      │
│   │   └─ Adaptive evolution steps                            │
│   │                                                           │
│   └─ Topology Monitor                                        │
│       ├─ Community structure quality (modularity Q)          │
│       ├─ Hub neuron identification                           │
│       └─ Connectivity health assessment                      │
└──────────────────────────────────────────────────────────────┘
```

## Enhanced Implementation Phases

---

## Phase 1.5.1: Event Bus + Shannon Infrastructure (Week 1)

**Goal**: Establish information-theoretic event routing with adaptive optimization

**Files to Create**:
- `include/middleware/integration/nimcp_cognitive_adapter.h`
- `src/middleware/integration/nimcp_cognitive_adapter.c`
- `include/middleware/integration/nimcp_shannon_monitor.h` ⭐ NEW
- `src/middleware/integration/nimcp_shannon_monitor.c` ⭐ NEW
- `include/middleware/integration/nimcp_flow_tracker.h` ⭐ NEW
- `src/middleware/integration/nimcp_flow_tracker.c` ⭐ NEW

### Enhanced Data Structures

```c
/**
 * @brief Shannon monitoring for event routing
 */
typedef struct {
    // Channel capacity monitoring
    float channel_capacity_bits_per_sec;   // C = B log₂(1 + SNR)
    float current_throughput;               // Actual bits/sec
    float capacity_utilization;             // throughput / capacity

    // Bottleneck detection
    bool bottleneck_detected;
    float bottleneck_severity;              // 0.0-1.0
    uint32_t bottleneck_location;           // Which module is bottlenecked

    // Information loss tracking
    float information_loss_rate;            // I(X;Y) / H(X)
    float filtered_bits_per_sec;            // Information dropped by filtering

    // Entropy measurements
    float event_entropy;                    // H(events)
    float cognitive_response_entropy;       // H(responses)
    float mutual_information;               // I(events;responses)

    // Statistics
    uint64_t total_events;
    uint64_t filtered_events;
    uint64_t bottlenecked_events;
} shannon_routing_metrics_t;

/**
 * @brief Cross-modal information flow tracking
 */
typedef struct {
    // Flow rates (bits/sec)
    float middleware_to_executive;
    float middleware_to_workspace;
    float middleware_to_introspection;
    float executive_to_middleware;
    float workspace_to_middleware;

    // Efficiency metrics
    float flow_efficiency[5];               // η = I_out / I_in per path
    float bottleneck_severity[5];           // 0.0-1.0 per path

    // Channel capacities
    float channel_capacity[5];              // Max bits/sec per path
    float capacity_utilization[5];          // Current / max per path

    // Latency tracking
    float avg_latency_us[5];                // Average latency per path
    float p99_latency_us[5];                // 99th percentile latency
} cross_modal_flow_metrics_t;

/**
 * @brief Enhanced cognitive event adapter with Shannon monitoring
 */
typedef struct {
    // Core routing
    executive_controller_t* executive;
    global_workspace_t* workspace;
    introspection_context_t introspection;

    // Shannon monitoring ⭐ NEW
    shannon_routing_metrics_t shannon_metrics;
    bool enable_shannon_monitoring;
    shannon_config_t shannon_config;

    // Cross-modal flow tracking ⭐ NEW
    cross_modal_flow_metrics_t flow_metrics;
    bool enable_flow_tracking;

    // Adaptive filtering based on information theory ⭐ NEW
    float info_threshold_bits;              // Min information content to pass
    float adaptive_filter_alpha;            // Adaptation rate (0.0-1.0)
    bool enable_adaptive_filtering;

    // Performance tracking
    uint64_t events_routed;
    uint64_t events_filtered;               // Low information content
    uint64_t events_bottlenecked;           // Dropped due to overload

    // Ring buffer for event history (for entropy calculation)
    middleware_event_t* event_history;
    uint32_t history_size;
    uint32_t history_index;
} cognitive_event_adapter_t;
```

### Enhanced APIs

```c
//=============================================================================
// Shannon Monitoring API
//=============================================================================

/**
 * @brief Enable Shannon information monitoring for event routing
 *
 * WHAT: Activate real-time channel capacity and bottleneck detection
 * WHY:  Ensure optimal information flow between middleware and cognitive layers
 * HOW:  Computes H(X), H(Y), I(X;Y) for events and responses
 *
 * PERFORMANCE: ~2-5µs overhead per event (entropy calculation)
 * MEMORY: ~4KB for statistics tracking
 *
 * @param adapter Cognitive event adapter
 * @param enable true to enable, false to disable
 */
void cognitive_adapter_enable_shannon_monitoring(
    cognitive_event_adapter_t* adapter,
    bool enable
);

/**
 * @brief Get current Shannon routing metrics
 *
 * @param adapter Cognitive event adapter
 * @return Shannon metrics snapshot
 */
shannon_routing_metrics_t cognitive_adapter_get_shannon_metrics(
    const cognitive_event_adapter_t* adapter
);

/**
 * @brief Calculate channel capacity for event routing
 *
 * WHAT: Compute maximum sustainable event rate
 * WHY:  Prevent cognitive module overload
 * HOW:  C = B log₂(1 + SNR) where B = bandwidth, SNR = signal-to-noise
 *
 * @param adapter Cognitive event adapter
 * @return Channel capacity in bits/second
 */
float cognitive_adapter_calculate_channel_capacity(
    const cognitive_event_adapter_t* adapter
);

/**
 * @brief Detect information bottlenecks in routing
 *
 * WHAT: Identify if any cognitive module is overloaded
 * WHY:  Prevent information loss and latency spikes
 * HOW:  Checks if I(X;Y) < threshold, indicating information loss
 *
 * @param adapter Cognitive event adapter
 * @param[out] bottleneck_module Which module is bottlenecked
 * @return Bottleneck severity (0.0 = none, 1.0 = severe)
 */
float cognitive_adapter_detect_bottleneck(
    const cognitive_event_adapter_t* adapter,
    cognitive_module_t* bottleneck_module
);

//=============================================================================
// Cross-Modal Flow Tracking API
//=============================================================================

/**
 * @brief Enable cross-modal information flow tracking
 *
 * WHAT: Monitor information flow between middleware and cognitive layers
 * WHY:  Understand and optimize layer integration efficiency
 * HOW:  Tracks bits/sec, latency, and efficiency for each path
 *
 * PERFORMANCE: ~1-2µs overhead per event
 * MEMORY: ~2KB for flow statistics
 *
 * @param adapter Cognitive event adapter
 * @param enable true to enable, false to disable
 */
void cognitive_adapter_enable_flow_tracking(
    cognitive_event_adapter_t* adapter,
    bool enable
);

/**
 * @brief Get cross-modal flow metrics
 *
 * @param adapter Cognitive event adapter
 * @return Flow metrics snapshot
 */
cross_modal_flow_metrics_t cognitive_adapter_get_flow_metrics(
    const cognitive_event_adapter_t* adapter
);

/**
 * @brief Calculate flow efficiency for a path
 *
 * WHAT: Measure how much input information reaches output
 * WHY:  Identify lossy paths that need optimization
 * HOW:  η = I_out / I_in = mutual_information / input_entropy
 *
 * @param adapter Cognitive event adapter
 * @param path Which path to measure
 * @return Efficiency [0.0-1.0] (1.0 = perfect, 0.0 = total loss)
 */
float cognitive_adapter_calculate_flow_efficiency(
    const cognitive_event_adapter_t* adapter,
    integration_path_t path
);

//=============================================================================
// Adaptive Filtering API (Information-Theoretic)
//=============================================================================

/**
 * @brief Enable adaptive information-based filtering
 *
 * WHAT: Automatically adjust filter threshold based on channel capacity
 * WHY:  Prevent overload while maximizing information throughput
 * HOW:  Adapts threshold to keep capacity_utilization < 0.8
 *
 * ALGORITHM:
 *   if (utilization > 0.8):
 *     threshold += alpha * (utilization - 0.8)
 *   else if (utilization < 0.5):
 *     threshold -= alpha * (0.5 - utilization)
 *
 * @param adapter Cognitive event adapter
 * @param enable true to enable, false to disable
 * @param alpha Adaptation rate [0.0-1.0] (default: 0.1)
 */
void cognitive_adapter_enable_adaptive_filtering(
    cognitive_event_adapter_t* adapter,
    bool enable,
    float alpha
);

/**
 * @brief Calculate information content of an event
 *
 * WHAT: Measure how many bits of information an event carries
 * WHY:  Filter low-information events to save processing
 * HOW:  I = -log₂(P(event)) where P is probability based on history
 *
 * @param adapter Cognitive event adapter
 * @param event Event to measure
 * @return Information content in bits
 */
float cognitive_adapter_measure_event_information(
    const cognitive_event_adapter_t* adapter,
    const middleware_event_t* event
);
```

### Enhanced Event Routing Implementation

```c
/**
 * @brief Enhanced event routing with Shannon monitoring
 */
void cognitive_adapter_route_event(
    cognitive_event_adapter_t* adapter,
    const middleware_event_t* event
) {
    uint64_t start_time_us = platform_get_time_us();

    // === SHANNON MONITORING ===
    float event_info_bits = 0.0f;
    if (adapter->enable_shannon_monitoring) {
        // Measure information content
        event_info_bits = cognitive_adapter_measure_event_information(
            adapter, event
        );

        // Update entropy statistics
        update_event_entropy_history(adapter, event);

        // Check channel capacity
        float capacity = cognitive_adapter_calculate_channel_capacity(adapter);
        float utilization = adapter->shannon_metrics.current_throughput / capacity;

        // Detect bottlenecks
        if (utilization > 0.8) {
            cognitive_module_t bottleneck_module;
            float severity = cognitive_adapter_detect_bottleneck(
                adapter, &bottleneck_module
            );

            if (severity > 0.5) {
                adapter->shannon_metrics.bottleneck_detected = true;
                adapter->shannon_metrics.bottleneck_location = bottleneck_module;
                adapter->shannon_metrics.bottleneck_severity = severity;
            }
        }
    }

    // === ADAPTIVE FILTERING ===
    if (adapter->enable_adaptive_filtering) {
        // Filter low-information events if approaching capacity
        if (adapter->shannon_metrics.capacity_utilization > 0.8) {
            if (event_info_bits < adapter->info_threshold_bits) {
                adapter->events_filtered++;
                adapter->shannon_metrics.filtered_bits_per_sec += event_info_bits;

                // Update flow metrics
                if (adapter->enable_flow_tracking) {
                    record_filtered_event_in_flow_metrics(adapter, event);
                }

                return;  // Drop low-information event
            }
        }

        // Adapt threshold based on utilization
        adaptive_adjust_information_threshold(adapter);
    }

    // === ROUTE EVENT ===
    bool routed = false;

    switch (event->type) {
        case EVENT_TYPE_PATTERN_DETECTED:
            if (adapter->executive) {
                executive_on_pattern_detected(
                    adapter->executive,
                    &event->data.pattern
                );
                routed = true;

                // Track flow
                if (adapter->enable_flow_tracking) {
                    adapter->flow_metrics.middleware_to_executive += event_info_bits;
                }
            }
            break;

        case EVENT_TYPE_SALIENCE_PEAK:
            if (adapter->workspace) {
                global_workspace_on_salience_peak(
                    adapter->workspace,
                    &event->data.salience
                );
                routed = true;

                // Track flow
                if (adapter->enable_flow_tracking) {
                    adapter->flow_metrics.middleware_to_workspace += event_info_bits;
                }
            }
            break;

        case EVENT_TYPE_OSCILLATION_CHANGE:
            if (adapter->executive) {
                executive_on_oscillation_change(
                    adapter->executive,
                    &event->data.oscillation
                );
                routed = true;

                // Track flow
                if (adapter->enable_flow_tracking) {
                    adapter->flow_metrics.middleware_to_executive += event_info_bits;
                }
            }
            break;

        // ... other event types ...
    }

    // === UPDATE METRICS ===
    uint64_t end_time_us = platform_get_time_us();
    uint64_t latency_us = end_time_us - start_time_us;

    if (routed) {
        adapter->events_routed++;

        // Update Shannon metrics
        if (adapter->enable_shannon_monitoring) {
            adapter->shannon_metrics.current_throughput += event_info_bits;
            adapter->shannon_metrics.total_events++;
        }

        // Update flow metrics
        if (adapter->enable_flow_tracking) {
            update_flow_latency_statistics(adapter, event->type, latency_us);
        }
    }
}
```

### Adaptive Threshold Algorithm

```c
/**
 * @brief Adapt information threshold based on channel utilization
 *
 * ALGORITHM:
 *   Target: Keep utilization between 0.5 and 0.8
 *   If utilization > 0.8: Increase threshold (filter more)
 *   If utilization < 0.5: Decrease threshold (filter less)
 *
 *   threshold(t+1) = threshold(t) + α * error
 *   where error = (utilization - target)
 */
static void adaptive_adjust_information_threshold(
    cognitive_event_adapter_t* adapter
) {
    const float TARGET_UTILIZATION = 0.65f;  // Sweet spot
    const float MAX_THRESHOLD = 12.0f;       // Max bits (very rare events)
    const float MIN_THRESHOLD = 1.0f;        // Min bits (very common events)

    float utilization = adapter->shannon_metrics.capacity_utilization;
    float error = utilization - TARGET_UTILIZATION;

    // Proportional control with adaptive learning rate
    float adjustment = adapter->adaptive_filter_alpha * error;

    // Update threshold
    adapter->info_threshold_bits += adjustment;

    // Clamp to reasonable range
    if (adapter->info_threshold_bits > MAX_THRESHOLD) {
        adapter->info_threshold_bits = MAX_THRESHOLD;
    }
    if (adapter->info_threshold_bits < MIN_THRESHOLD) {
        adapter->info_threshold_bits = MIN_THRESHOLD;
    }
}
```

**Expected Results**:
- Self-optimizing event routing maintains 50-80% capacity utilization
- Bottleneck detection prevents cognitive module overload
- Information loss <5% under normal load
- Latency: <15µs overhead per event (including Shannon calculations)

**LOC Estimate**: ~600 lines (was 400 + 200 for Shannon/flow)

---

## Phase 1.5.2: Executive Integration + Quantum Command Propagation (Week 2)

**Goal**: Enable Executive to respond to patterns with quantum-accelerated command distribution

**Files to Modify**:
- `src/include/cognitive/nimcp_executive.h`
- `src/cognitive/executive/nimcp_executive.c`
- `src/middleware/patterns/nimcp_pattern_library.c`

**Files to Create**:
- `include/middleware/integration/nimcp_quantum_commander.h` ⭐ NEW
- `src/middleware/integration/nimcp_quantum_commander.c` ⭐ NEW

### Quantum Command Propagation

```c
/**
 * @brief Quantum-Shannon command propagation engine
 *
 * WHAT: Use quantum diffusion for O(√N) command distribution
 * WHY:  Executive decisions need to reach all middleware components quickly
 * HOW:  Quantum walk on brain network with Shannon bottleneck detection
 */
typedef struct {
    brain_t brain;
    quantum_shannon_diffusion_t* diffusion;

    // Configuration
    uint32_t evolution_steps;               // Quantum steps per command
    float mixing_ratio;                     // Quantum vs classical mix

    // Source neurons for different command types
    uint32_t executive_focal_neuron;        // Executive's "hub" neuron
    uint32_t attention_command_neuron;      // Attention control source
    uint32_t routing_command_neuron;        // Routing control source

    // Performance metrics
    float avg_propagation_time_us;
    float speedup_vs_classical;             // Measured √N speedup
    shannon_diffusion_metrics_t shannon_metrics;
} quantum_command_propagator_t;

/**
 * @brief Create quantum command propagator
 *
 * @param brain Brain handle
 * @param exec Executive controller (to identify focal neuron)
 * @return Propagator handle or NULL on error
 */
quantum_command_propagator_t* quantum_command_propagator_create(
    brain_t brain,
    executive_controller_t* exec
);

/**
 * @brief Propagate executive command using quantum diffusion
 *
 * WHAT: Distribute command to all middleware components in O(√N) time
 * WHY:  Fast response to executive decisions (critical for <200µs target)
 * HOW:  Quantum walk from executive focal neuron with information payload
 *
 * COMPLEXITY: O(√N) vs O(N) for classical broadcast
 * SPEEDUP: ~31x for N=1000, ~100x for N=10000
 *
 * @param propagator Quantum propagator
 * @param command_type Type of command
 * @param command_data Command payload
 * @param information_bits Information content of command
 * @return true on success
 */
bool quantum_propagate_executive_command(
    quantum_command_propagator_t* propagator,
    executive_command_type_t command_type,
    const void* command_data,
    float information_bits
);
```

### Executive Enhanced with Oscillations + Quantum Propagation

```c
/**
 * @brief Executive responds to oscillation changes (ENHANCED)
 *
 * NOW WITH: Oscillation-driven mode switching + quantum command propagation
 */
void executive_on_oscillation_change(
    executive_controller_t* exec,
    const oscillation_change_data_t* oscillation
) {
    // === BIOLOGICAL MODE SWITCHING ===

    // Theta oscillations (4-8 Hz) → Memory encoding/consolidation
    if (oscillation->band == THETA_BAND && oscillation->power > 0.7) {
        executive_switch_mode(exec, EXECUTIVE_MODE_MEMORY_ENCODING);

        // Configure middleware for memory task
        middleware_command_t cmd = {
            .type = COMMAND_CONFIGURE_ATTENTION,
            .target = REGION_HIPPOCAMPUS,
            .priority = 0.9f,  // High priority for hippocampal signals
            .selectivity = 0.6f  // Moderate selectivity
        };

        // Quantum-accelerated command propagation ⭐
        if (exec->quantum_propagator) {
            quantum_propagate_executive_command(
                exec->quantum_propagator,
                cmd.type,
                &cmd,
                8.0f  // Information content: mode switch + config
            );
        }
    }

    // Gamma oscillations (30-100 Hz) → Focused attention
    else if (oscillation->band == GAMMA_BAND && oscillation->power > 0.8) {
        executive_switch_mode(exec, EXECUTIVE_MODE_FOCUSED_ATTENTION);

        middleware_command_t cmd = {
            .type = COMMAND_CONFIGURE_ATTENTION,
            .target = REGION_PREFRONTAL,
            .priority = 0.95f,  // Very high selectivity
            .selectivity = 0.95f  // Filter almost everything
        };

        // Fast propagation for time-critical attention shifts ⭐
        quantum_propagate_executive_command(
            exec->quantum_propagator,
            cmd.type,
            &cmd,
            6.0f
        );
    }

    // Alpha oscillations (8-12 Hz) → Idle/relaxed state
    else if (oscillation->band == ALPHA_BAND && oscillation->power > 0.7) {
        executive_switch_mode(exec, EXECUTIVE_MODE_IDLE);

        middleware_command_t cmd = {
            .type = COMMAND_REDUCE_ACTIVITY,
            .priority = 0.3f,  // Low priority
            .selectivity = 0.4f  // More permissive
        };

        quantum_propagate_executive_command(
            exec->quantum_propagator,
            cmd.type,
            &cmd,
            4.0f
        );
    }
}

/**
 * @brief Executive responds to pattern detection
 *
 * ENHANCED: Shannon information content guides task prioritization
 */
void executive_on_pattern_detected(
    executive_controller_t* exec,
    const pattern_detected_data_t* pattern
) {
    // Measure pattern information content ⭐
    float pattern_info_bits = shannon_calculate_pattern_information(
        pattern->pattern_id,
        pattern->match_confidence
    );

    // High-information patterns get higher task priority
    task_priority_t priority;
    if (pattern_info_bits > 10.0f) {
        priority = PRIORITY_URGENT;  // Very informative pattern
    } else if (pattern_info_bits > 6.0f) {
        priority = PRIORITY_HIGH;
    } else {
        priority = PRIORITY_NORMAL;
    }

    // Create task based on pattern
    task_descriptor_t task = {
        .type = TASK_TYPE_SEQUENCE,
        .priority = priority,
        .name = pattern->pattern_name,
        .context = (void*)pattern
    };

    executive_add_task(exec, &task);

    // If urgent, reconfigure attention immediately
    if (priority == PRIORITY_URGENT) {
        middleware_command_t cmd = {
            .type = COMMAND_SUBSCRIBE_PATTERN,
            .pattern_id = pattern->pattern_id,
            .priority = 0.9f
        };

        quantum_propagate_executive_command(
            exec->quantum_propagator,
            cmd.type,
            &cmd,
            pattern_info_bits  // Propagate same info content
        );
    }
}
```

**Expected Results**:
- Oscillation-driven mode switching (biological plausibility)
- Quantum command propagation: ~31x faster for N=1000 neurons
- Shannon-guided task prioritization
- Command latency: <50µs (vs ~1ms classical broadcast)

**LOC Estimate**: ~450 lines (was 300 + 150 for quantum)

---

## Phase 1.5.3: Global Workspace Integration + Information Competition (Week 3)

**Goal**: Shannon information content determines workspace competition and broadcast

**Files to Modify**:
- `src/cognitive/global_workspace/nimcp_global_workspace.c`
- `src/middleware/patterns/nimcp_pattern_library.c`

### Enhanced Global Workspace Competition

```c
/**
 * @brief Global workspace competition based on information content
 *
 * ENHANCEMENT: Competition strength = salience × information_content
 * WHY: High-information, salient events should win consciousness
 */
bool global_workspace_on_salience_peak(
    global_workspace_t* workspace,
    const salience_peak_data_t* peak
) {
    // === SHANNON INFORMATION CONTENT ===

    // Measure information content of salient event ⭐
    float info_bits = shannon_measure_feature_information(
        peak->feature_vector,
        peak->dimension
    );

    // Competition strength = salience × information content
    float competition_strength = peak->salience_score * (info_bits / 10.0f);

    // Clamp to [0, 1]
    if (competition_strength > 1.0f) competition_strength = 1.0f;

    // Convert feature vector to workspace content
    float content[GLOBAL_WORKSPACE_DEFAULT_DIM];
    feature_vector_to_workspace_content(
        peak->feature_vector,
        peak->dimension,
        content,
        GLOBAL_WORKSPACE_DEFAULT_DIM
    );

    // === COMPETITION ===

    bool won_competition = global_workspace_compete(
        workspace,
        MODULE_SALIENCE,
        content,
        GLOBAL_WORKSPACE_DEFAULT_DIM,
        competition_strength  // Information-weighted competition
    );

    if (won_competition) {
        // Event reached consciousness!
        // Broadcast to all subscribers

        // === SHANNON-MONITORED BROADCAST ===
        shannon_broadcast_metrics_t broadcast_metrics =
            global_workspace_broadcast_with_shannon(
                workspace,
                content,
                GLOBAL_WORKSPACE_DEFAULT_DIM,
                info_bits  // Track information flow
            );

        // Check if any subscriber is bottlenecked
        if (broadcast_metrics.bottleneck_detected) {
            // Reduce broadcast rate temporarily
            global_workspace_set_broadcast_rate(
                workspace,
                0.5f * workspace->current_broadcast_rate
            );
        }
    }

    return won_competition;
}

/**
 * @brief Shannon-monitored broadcast to subscribers
 *
 * WHAT: Broadcast with channel capacity monitoring per subscriber
 * WHY:  Prevent subscriber overload
 * HOW:  Tracks I(broadcast;response) for each subscriber
 */
typedef struct {
    bool bottleneck_detected;
    cognitive_module_t bottlenecked_module;
    float information_delivered[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS];
    float information_loss[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS];
} shannon_broadcast_metrics_t;

shannon_broadcast_metrics_t global_workspace_broadcast_with_shannon(
    global_workspace_t* workspace,
    const float* content,
    uint32_t dim,
    float content_info_bits
) {
    shannon_broadcast_metrics_t metrics = {0};

    // Broadcast to each subscriber
    for (uint32_t i = 0; i < workspace->num_subscribers; i++) {
        cognitive_module_t subscriber = workspace->subscribers[i];

        // Check subscriber channel capacity
        float subscriber_capacity =
            global_workspace_get_subscriber_capacity(workspace, subscriber);
        float subscriber_load =
            global_workspace_get_subscriber_load(workspace, subscriber);

        float utilization = subscriber_load / subscriber_capacity;

        if (utilization > 0.9f) {
            // Subscriber is overloaded - reduce information delivery
            metrics.bottleneck_detected = true;
            metrics.bottlenecked_module = subscriber;

            // Deliver at reduced rate
            float delivered = content_info_bits * (1.0f - utilization);
            metrics.information_delivered[i] = delivered;
            metrics.information_loss[i] = content_info_bits - delivered;
        } else {
            // Full delivery
            metrics.information_delivered[i] = content_info_bits;
            metrics.information_loss[i] = 0.0f;
        }

        // Notify subscriber (async)
        global_workspace_notify_subscriber(
            workspace,
            subscriber,
            content,
            dim
        );
    }

    return metrics;
}
```

**Expected Results**:
- Information-weighted competition (high-info events win)
- Shannon-monitored broadcasts (prevents subscriber overload)
- Adaptive broadcast rate (reduces on bottleneck)
- Information loss <5% to subscribers

**LOC Estimate**: ~320 lines (was 250 + 70 for Shannon)

---

## Phase 1.5.4: Introspection + Community Detection Health Monitoring (Week 4)

**Goal**: Introspection monitors signal quality, connectivity health, and hub neuron status

**Files to Modify**:
- `src/cognitive/introspection/nimcp_introspection.c`
- `src/middleware/buffering/nimcp_integration_buffer.c`

**Files to Create**:
- `include/cognitive/introspection/nimcp_connectivity_health.h` ⭐ NEW
- `src/cognitive/introspection/nimcp_connectivity_health.c` ⭐ NEW

### Enhanced Introspection with Topology Monitoring

```c
/**
 * @brief Brain connectivity health assessment
 */
typedef struct {
    // Community structure quality
    float modularity_Q;                     // Modularity score [0-1]
    uint32_t num_communities;               // Number detected
    float community_balance;                // Size distribution balance

    // Hub neuron analysis
    uint32_t num_hub_neurons;              // Total hubs detected
    uint32_t* hub_neuron_ids;              // Hub IDs
    float* hub_centrality;                 // Centrality scores
    bool executive_has_hubs;               // Executive region has hubs?
    bool workspace_has_hubs;               // Workspace region has hubs?

    // Connectivity metrics
    float avg_clustering_coefficient;      // Local connectivity
    float avg_path_length;                 // Global connectivity
    float small_world_sigma;               // σ = (C/C_rand) / (L/L_rand)

    // Integration health
    float middleware_cognitive_connectivity; // Connection strength
    float information_flow_efficiency;      // η = I_out / I_in

    // Overall health score [0-1]
    float overall_health;
} brain_connectivity_health_t;

/**
 * @brief Assess brain connectivity health (introspection)
 *
 * WHAT: Full topology analysis with community detection + hub identification
 * WHY:  Self-awareness of brain's organizational quality
 * HOW:  Louvain community detection + degree centrality + graph metrics
 *
 * COMPLEXITY: O(N log N) for community detection, O(N²) for path length
 * LATENCY: ~10-50ms for N=1000 neurons (not real-time, periodic check)
 *
 * @param introspection Introspection context
 * @param brain Brain handle
 * @return Connectivity health assessment
 */
brain_connectivity_health_t introspection_assess_connectivity_health(
    introspection_context_t introspection,
    brain_t brain
);

/**
 * @brief Implementation of connectivity health assessment
 */
brain_connectivity_health_t introspection_assess_connectivity_health(
    introspection_context_t introspection,
    brain_t brain
) {
    brain_connectivity_health_t health = {0};

    // === COMMUNITY DETECTION ===

    // Run Louvain algorithm
    bool success = brain_detect_communities(brain);
    if (!success) {
        health.overall_health = 0.0f;
        return health;
    }

    // Get modularity score (Q ∈ [0, 1])
    health.modularity_Q = brain_get_modularity(brain);
    health.num_communities = brain_get_num_communities(brain);

    // Assess community size balance (Shannon entropy of sizes)
    uint32_t* community_sizes = brain_get_community_sizes(brain);
    health.community_balance = calculate_community_size_entropy(
        community_sizes,
        health.num_communities
    );

    // === HUB DETECTION ===

    // Identify hub neurons
    success = brain_detect_hubs(brain);
    if (success) {
        health.num_hub_neurons = brain_get_num_hubs(brain);
        health.hub_neuron_ids = brain_get_hub_ids(brain, &health.num_hub_neurons);
        health.hub_centrality = brain_get_hub_centrality(brain, &health.num_hub_neurons);

        // Check if key cognitive regions have hubs
        health.executive_has_hubs = check_region_has_hubs(
            brain, health.hub_neuron_ids, health.num_hub_neurons, REGION_EXECUTIVE
        );
        health.workspace_has_hubs = check_region_has_hubs(
            brain, health.hub_neuron_ids, health.num_hub_neurons, REGION_WORKSPACE
        );
    }

    // === TOPOLOGY METRICS ===

    // Compute graph metrics
    topology_metrics_t metrics = brain_compute_topology_metrics(brain);
    health.avg_clustering_coefficient = metrics.clustering_coefficient;
    health.avg_path_length = metrics.avg_path_length;
    health.small_world_sigma = metrics.small_world_coefficient;

    // === INTEGRATION HEALTH ===

    // Measure middleware-cognitive connectivity strength
    health.middleware_cognitive_connectivity =
        measure_layer_connectivity(brain, LAYER_MIDDLEWARE, LAYER_COGNITIVE);

    // Information flow efficiency from Shannon metrics
    shannon_network_metrics_t shannon = brain_get_shannon_metrics(brain);
    health.information_flow_efficiency = shannon.transfer_efficiency;

    // === OVERALL HEALTH SCORE ===

    // Weighted combination of all factors
    float modularity_score = health.modularity_Q;  // Good: Q > 0.3
    float hub_score = (health.executive_has_hubs && health.workspace_has_hubs) ? 1.0f : 0.5f;
    float small_world_score = (health.small_world_sigma > 1.0f) ? 1.0f : health.small_world_sigma;
    float flow_score = health.information_flow_efficiency;

    health.overall_health = (
        0.3f * modularity_score +
        0.2f * hub_score +
        0.2f * small_world_score +
        0.3f * flow_score
    );

    return health;
}

/**
 * @brief Assess signal quality via Shannon metrics
 *
 * ENHANCED: Now includes Shannon entropy and mutual information
 */
brain_signal_quality_t introspection_assess_signal_quality(
    introspection_context_t introspection,
    brain_temporal_buffer_t* temporal_buffer
) {
    brain_signal_quality_t quality = {0};

    // === TEMPORAL STATISTICS ===

    // Query sliding window statistics
    window_stats_t stats = sliding_window_get_stats(temporal_buffer->window);

    // Stability = low variance relative to mean
    float signal_stability = 1.0f - fminf(stats.stddev / (stats.mean + 1e-6f), 1.0f);

    // Multi-scale consistency
    float fast_power = integration_buffer_get_power(
        temporal_buffer->multiscale, TIMESCALE_FAST
    );
    float slow_power = integration_buffer_get_power(
        temporal_buffer->multiscale, TIMESCALE_SLOW
    );
    float scale_consistency = 1.0f - fabsf(fast_power - slow_power);

    // === SHANNON INFORMATION METRICS ⭐ ===

    // Signal entropy (diversity of values)
    float signal_entropy = shannon_calculate_entropy(
        stats.samples,
        stats.count
    );

    // Predictability = low entropy → high predictability
    float max_entropy = log2f((float)stats.count);  // Maximum possible
    float signal_predictability = 1.0f - (signal_entropy / max_entropy);

    // Information rate (bits/sample)
    float information_rate = signal_entropy;

    // === COMBINED QUALITY ===

    quality.stability = signal_stability;
    quality.consistency = scale_consistency;
    quality.predictability = signal_predictability;  // ⭐ NEW
    quality.information_rate = information_rate;     // ⭐ NEW
    quality.overall_quality = (
        0.3f * signal_stability +
        0.3f * scale_consistency +
        0.2f * signal_predictability +
        0.2f * (information_rate / max_entropy)  // Normalize
    );

    return quality;
}
```

**Expected Results**:
- Full topology awareness (communities, hubs, small-world metrics)
- Shannon-enhanced signal quality assessment
- Periodic health checks (every 10 seconds, not real-time)
- Latency: <50ms for full assessment (10,000 neurons)

**LOC Estimate**: ~350 lines (was 200 + 150 for community detection)

---

## Phase 1.5.5: Command Interface + Quantum Annealing Optimization (Week 5)

**Goal**: Cognitive modules configure middleware, with quantum annealing for routing optimization

**Files to Create**:
- `include/middleware/integration/nimcp_middleware_controller.h`
- `src/middleware/integration/nimcp_middleware_controller.c`
- `include/middleware/integration/nimcp_routing_optimizer.h` ⭐ NEW
- `src/middleware/integration/nimcp_routing_optimizer.c` ⭐ NEW

### Quantum Annealing for Routing Optimization

```c
/**
 * @brief Routing optimization problem for quantum annealing
 */
typedef struct {
    // Optimization objectives (multi-objective)
    float weight_minimize_latency;          // [0-1]
    float weight_maximize_throughput;       // [0-1]
    float weight_balance_load;              // [0-1]

    // Constraints
    float max_latency_us;                   // Hard limit
    float min_throughput_events_per_sec;    // Hard limit
    float max_load_imbalance;               // Soft limit

    // Current routing configuration
    routing_table_t* current_routing;

    // Performance history (for learning)
    float historical_latencies[100];
    float historical_throughputs[100];
    uint32_t history_count;
} routing_optimization_problem_t;

/**
 * @brief Use quantum annealing to optimize event routing
 *
 * WHAT: Find globally optimal routing configuration
 * WHY:  Escape local minima in routing configuration space
 * HOW:  Map routing to Ising model, use quantum annealing
 *
 * COMPLEXITY: O(2^N) state space, but quantum annealing explores efficiently
 * LATENCY: ~100ms for optimization (not real-time, periodic)
 *
 * @param annealer Quantum annealer handle
 * @param problem Routing optimization problem
 * @return Optimal routing configuration
 */
routing_table_t* quantum_anneal_routing_optimization(
    quantum_annealer_t* annealer,
    const routing_optimization_problem_t* problem
);

/**
 * @brief Middleware controller with quantum-optimized routing
 */
typedef struct {
    // Middleware components
    thalamic_router_t* router;
    attention_gate_t* attention_gates;
    pattern_library_t* patterns;
    feature_extractor_t* features;

    // Quantum optimization ⭐
    quantum_annealer_t* annealer;
    routing_optimization_problem_t* optimization_problem;
    bool enable_quantum_optimization;
    uint64_t last_optimization_time_ms;
    uint64_t optimization_interval_ms;      // Default: 60000 (1 min)

    // Performance tracking for optimization
    float recent_latencies[100];
    float recent_throughputs[100];
    uint32_t perf_history_index;
} middleware_controller_t;

/**
 * @brief Periodically optimize routing with quantum annealing
 *
 * WHAT: Background optimization of routing tables
 * WHY:  Continuously adapt to changing workload patterns
 * HOW:  Collect performance data, run quantum annealing, apply new routing
 *
 * FREQUENCY: Every 1-5 minutes (configurable)
 * COST: ~100ms CPU time per optimization
 *
 * @param controller Middleware controller
 */
void middleware_controller_periodic_optimization(
    middleware_controller_t* controller
) {
    if (!controller->enable_quantum_optimization) return;

    uint64_t now_ms = platform_get_time_ms();
    if (now_ms - controller->last_optimization_time_ms <
        controller->optimization_interval_ms) {
        return;  // Not time yet
    }

    // === COLLECT PERFORMANCE DATA ===

    // Update optimization problem with recent performance
    controller->optimization_problem->history_count = 100;
    memcpy(
        controller->optimization_problem->historical_latencies,
        controller->recent_latencies,
        100 * sizeof(float)
    );
    memcpy(
        controller->optimization_problem->historical_throughputs,
        controller->recent_throughputs,
        100 * sizeof(float)
    );

    // === QUANTUM ANNEALING ===

    routing_table_t* optimal_routing = quantum_anneal_routing_optimization(
        controller->annealer,
        controller->optimization_problem
    );

    if (optimal_routing) {
        // Apply new routing configuration
        thalamic_router_set_routing_table(
            controller->router,
            optimal_routing
        );

        // Update timestamp
        controller->last_optimization_time_ms = now_ms;
    }
}
```

**Expected Results**:
- Quantum-optimized routing tables (globally optimal)
- Continuous adaptation to workload changes
- Optimization latency: ~100ms (background, not blocking)
- Performance improvement: 10-30% over manual configuration

**LOC Estimate**: ~400 lines (was 300 + 100 for quantum)

---

## Enhanced Performance Targets

| Operation | Enhanced Target | Original | Improvement |
|-----------|----------------|----------|-------------|
| Event routing | <15µs | <10µs | +5µs (Shannon overhead) |
| Shannon monitoring | ~2-5µs | N/A | NEW |
| Flow tracking | ~1-2µs | N/A | NEW |
| Executive event handling | <50µs | <50µs | Same |
| Quantum command propagation | <50µs | ~1ms | **20x faster** |
| Workspace competition | <100µs | <100µs | Same |
| Introspection query | <20µs | <20µs | Same |
| **Total overhead per event** | **<220µs** | **<200µs** | +20µs (acceptable) |
| Community detection | <50ms | N/A | NEW (periodic) |
| Quantum routing optimization | <100ms | N/A | NEW (periodic) |

## Enhanced Memory Overhead

| Component | Enhanced | Original | Addition |
|-----------|----------|----------|----------|
| Cognitive adapter | 2 KB | 512 bytes | +1.5 KB (Shannon/flow) |
| Event router | 2 KB | 2 KB | Same |
| Middleware controller | 1 KB | 1 KB | Same |
| Shannon monitor | 4 KB | N/A | +4 KB |
| Flow tracker | 2 KB | N/A | +2 KB |
| Quantum propagator | 8 KB | N/A | +8 KB |
| Connectivity health | 2 KB | N/A | +2 KB |
| **Total** | **~21 KB** | **<4 KB** | **+17 KB** |

**Impact**: Still minimal (<0.1% of typical brain memory)

---

## Enhanced Testing Strategy

### Unit Tests (Mathematical Verification)

```c
// Shannon monitoring tests
test_shannon_channel_capacity_calculation()
test_shannon_bottleneck_detection()
test_shannon_information_loss_tracking()
test_adaptive_filtering_convergence()

// Cross-modal flow tests
test_flow_tracking_accuracy()
test_flow_efficiency_calculation()
test_multi_path_flow_monitoring()

// Quantum propagation tests
test_quantum_command_speedup()  // Verify √N speedup
test_quantum_shannon_bottleneck_detection()
test_quantum_classical_mixing()

// Community detection tests
test_community_detection_modularity()
test_hub_neuron_identification()
test_connectivity_health_assessment()

// Quantum annealing tests
test_routing_optimization_convergence()
test_quantum_annealing_vs_classical()
test_multi_objective_optimization()
```

### Integration Tests (End-to-End)

```c
// Information-theoretic integration
test_high_info_events_prioritized()
test_low_info_events_filtered_under_load()
test_bottleneck_triggers_adaptive_filtering()
test_flow_efficiency_matches_theory()

// Quantum command propagation
test_quantum_command_reaches_all_gates()
test_quantum_speedup_vs_classical()
test_shannon_bottleneck_during_propagation()

// Oscillation-driven behavior
test_theta_triggers_memory_mode()
test_gamma_triggers_attention_mode()
test_mode_switch_propagates_quantum()

// Topology-driven behavior
test_hub_neurons_preferred_for_routing()
test_poor_modularity_triggers_reorg()
test_connectivity_health_affects_performance()
```

### Performance Benchmarks (Mathematical Guarantees)

```c
// Shannon guarantees
benchmark_channel_capacity_adherence()      // Throughput ≤ C
benchmark_information_loss_minimization()   // Loss < 5%
benchmark_bottleneck_detection_accuracy()   // Precision/recall > 0.9

// Quantum speedup
benchmark_quantum_propagation_speedup()     // Measure actual √N
benchmark_quantum_vs_classical_latency()    // Target: 20x faster

// Flow efficiency
benchmark_layer_integration_efficiency()    // η > 0.9 under normal load
benchmark_multi_path_flow_balance()        // Variance < 0.1

// Topology metrics
benchmark_community_detection_speed()       // <50ms for N=10000
benchmark_hub_identification_accuracy()     // Match ground truth
benchmark_modularity_optimization()         // Q increases over time
```

---

## Enhanced Implementation Costs

| Phase | Enhanced LOC | Original LOC | Addition | Duration |
|-------|-------------|--------------|----------|----------|
| 1.5.1 | 600 | 400 | +200 | 1.5 weeks |
| 1.5.2 | 450 | 300 | +150 | 1.5 weeks |
| 1.5.3 | 320 | 250 | +70 | 1 week |
| 1.5.4 | 350 | 200 | +150 | 1.5 weeks |
| 1.5.5 | 400 | 300 | +100 | 1.5 weeks |
| **Total** | **~2120** | **~1450** | **+670** | **~7.5 weeks** |

**Note**: Additional 1.5 weeks for mathematical enhancements (vs 5 weeks original)

---

## Expected Benefits (Enhanced)

### Original Benefits
1. ✅ Unified cognitive architecture
2. ✅ Emergent cognitive phenomena
3. ✅ Performance efficiency
4. ✅ Biological plausibility
5. ✅ Extensibility

### NEW Mathematical Benefits
6. ✅ **Information-Theoretic Guarantees**
   - Channel capacity adherence prevents overload
   - Bottleneck detection with <5% false positive rate
   - Adaptive filtering maintains optimal throughput

7. ✅ **Quantum Speedup**
   - 20-100x faster command propagation (topology-dependent)
   - Real-time Shannon bottleneck detection during diffusion
   - Scalable to large networks (√N vs N)

8. ✅ **Self-Optimization**
   - Adaptive filtering based on channel utilization
   - Quantum annealing finds global routing optima
   - Continuous performance improvement

9. ✅ **Topology Awareness**
   - Community detection reveals functional organization
   - Hub neuron identification for preferential routing
   - Connectivity health monitoring for debugging

10. ✅ **Measurable Efficiency**
    - Information flow efficiency η > 0.9 under normal load
    - Cross-modal flow visualization
    - Data-driven performance tuning

---

## Enhanced Success Criteria

### Functional (Enhanced)
1. ✅ All original functional criteria
2. ✅ Shannon monitoring detects bottlenecks with <5% false positives
3. ✅ Adaptive filtering maintains 50-80% capacity utilization
4. ✅ Quantum command propagation shows √N speedup
5. ✅ Community detection runs in <50ms for N=10000
6. ✅ Flow efficiency η > 0.9 under normal load

### Performance (Enhanced)
1. ✅ Event routing: <15µs (was <10µs, +5µs acceptable for Shannon)
2. ✅ Quantum propagation: <50µs (vs ~1ms classical, **20x faster**)
3. ✅ Total overhead: <220µs per event (was <200µs, +20µs acceptable)
4. ✅ Memory overhead: <21 KB (was <4 KB, still negligible)

### Mathematical Guarantees
1. ✅ Throughput ≤ channel capacity C (Shannon limit)
2. ✅ Information loss <5% under normal load
3. ✅ Speedup ≥ 0.9 × √N for quantum propagation
4. ✅ Modularity Q increases over time with quantum annealing
5. ✅ Flow efficiency η converges to >0.9

---

## Recommended Start: Phase 1.5.1 Enhanced

**Immediate Action**: Begin with Phase 1.5.1 (Event Bus + Shannon + Flow Tracking)

**Rationale**:
- Creates mathematical foundation for all other phases
- Shannon monitoring provides immediate diagnostic value
- Flow tracking enables data-driven optimization
- ~600 LOC, 1.5 weeks estimate

**First Milestone**: Self-optimizing event router with bottleneck detection

---

**Questions for Discussion**:

1. **Shannon Monitoring Overhead**: Accept +5µs overhead for bottleneck detection?
2. **Quantum Command Propagation**: Worth 20x speedup for +1.5 weeks implementation?
3. **Community Detection Frequency**: How often to run topology analysis? (Every 10s? 60s?)
4. **Quantum Annealing Cost**: Worth ~100ms optimization every 1-5 minutes?

Ready to proceed with **Phase 1.5.1 Enhanced**?
