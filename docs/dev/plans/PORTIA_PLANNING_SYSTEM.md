# Portia Spider Planning System

## Overview

The **Portia Planning System** is a memory-constrained route planning system inspired by the remarkable cognitive abilities of Portia spiders. These spiders can plan complex detours where targets are temporarily out of sight, all while working with extremely limited neural capacity.

## Biological Inspiration

### Portia Spider Capabilities

Portia spiders are famous for:
- **Complex route planning** with limited working memory
- **Blind navigation** where prey is temporarily invisible
- **Detour planning** requiring mental simulation
- **Backtracking** when routes fail
- **Opportunistic replanning** when better routes appear

### NIMCP Implementation Mapping

| Portia Spider Behavior | NIMCP Implementation |
|------------------------|---------------------|
| Visual scanning | Plan scanning/evaluation state |
| Limited working memory | Configurable waypoint limit |
| Detour planning (target unseen) | Detour depth tracking |
| Confidence in target location | Waypoint confidence decay |
| Opportunistic replanning | Dynamic re-evaluation |
| Backtracking when blocked | Obstacle handling & backtrack |

## Architecture

```
╔═══════════════════════════════════════════════════════════════════╗
║                    PORTIA PLANNING SYSTEM                          ║
║  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ ║
║  │   Scan      │→ │  Evaluate   │→ │   Execute   │→ │ Complete │ ║
║  │  Environment│  │   Routes    │  │   Steps     │  │          │ ║
║  └─────────────┘  └─────────────┘  └──────┬──────┘  └──────────┘ ║
║                                            │                       ║
║                    ┌──────────────────────┐                       ║
║                    │  Obstacle Handler    │                       ║
║                    │  (Detour/Backtrack)  │                       ║
║                    └──────────────────────┘                       ║
╚═══════════════════════════════════════════════════════════════════╝
```

## Key Features

### 1. Memory-Constrained Planning
- Configurable maximum waypoints per plan (default: 16)
- Mimics Portia spider's limited working memory
- Forces efficient route selection

### 2. Confidence-Based Waypoints
- Each waypoint has confidence value (0.0-1.0)
- Confidence decays exponentially with time since last "seen"
- Visibility threshold determines if waypoint is actionable

### 3. Blind Planning (Detours)
- Can navigate through invisible waypoints
- Detour depth tracking prevents excessive blind navigation
- Configurable max detour depth (default: 3)

### 4. Obstacle Handling
- Dynamic replanning when obstacles encountered
- Backtracking support to previous waypoints
- Plan failure when no viable alternatives exist

### 5. Bio-Async Integration
- Events broadcast on SEROTONIN channel (state coordination)
- Plan creation, completion, and failure events
- Integration with NIMCP's biological signaling system

### 6. Thread-Safe Operations
- All operations protected by mutex
- Thread-local error reporting
- Safe concurrent plan management

## API Reference

### Initialization

```c
portia_planning_config_t config = {
    .max_waypoints = 16,           // Memory limit
    .max_plans = 4,                // Concurrent plans
    .max_detour_depth = 3,         // Blind navigation limit
    .scan_interval_s = 0.1f,       // Re-evaluation rate
    .confidence_threshold = 0.6f,  // Min confidence to act
    .enable_backtracking = true    // Allow backtracking
};

portia_planner_t planner = portia_planning_init(&config, bio_ctx);
```

### Plan Creation

```c
// Create plan to target position
portia_plan_t* plan = portia_planning_create_plan(
    planner,
    target_x,
    target_y,
    target_z
);

// Add intermediate waypoints
portia_planning_add_waypoint(
    planner,
    plan->id,
    waypoint_x,
    waypoint_y,
    waypoint_z,
    confidence  // 0.0-1.0
);
```

### Plan Execution

```c
// Evaluate plan validity
bool valid = portia_planning_evaluate(planner, plan->id);

// Execute next step
bool success = portia_planning_execute_step(planner, plan->id);

// Check completion
plan_state_t state = portia_planning_get_state(planner, plan->id);
if (state == PLAN_STATE_COMPLETE) {
    printf("Plan completed!\n");
}
```

### Obstacle Handling

```c
// Handle obstacle at position
bool handled = portia_planning_handle_obstacle(
    planner,
    plan->id,
    obstacle_x,
    obstacle_y,
    obstacle_z
);

if (handled) {
    // Plan adapted - backtracked or replanned
} else {
    // Plan failed - no viable alternatives
}
```

### Detour Checking

```c
// Check if blind navigation is feasible
bool can_detour = portia_planning_can_detour(planner, plan->id);

if (can_detour) {
    // Can proceed through invisible waypoints
} else {
    // Too many invisible waypoints - detour not safe
}
```

## Plan States

| State | Description |
|-------|-------------|
| `PLAN_STATE_IDLE` | Plan created but not started |
| `PLAN_STATE_SCANNING` | Scanning environment for routes |
| `PLAN_STATE_EVALUATING` | Evaluating route options |
| `PLAN_STATE_EXECUTING` | Executing current plan |
| `PLAN_STATE_DETOUR` | Blind navigation through invisible waypoints |
| `PLAN_STATE_COMPLETE` | Plan completed successfully |
| `PLAN_STATE_FAILED` | Plan failed (no viable alternatives) |

## Confidence Decay

Waypoint confidence decays exponentially with time:

```
confidence(t) = confidence(0) * exp(-0.693 * t / half_life)
```

Where:
- `t` = time since last seen (seconds)
- `half_life` = 1.0 second (default)

This models the Portia spider's decreasing certainty about unseen locations.

## Usage Examples

### Example 1: Simple Path

```c
portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
portia_planning_execute_step(planner, plan->id);
// Plan completes immediately for single waypoint
```

### Example 2: Multi-Waypoint Route

```c
portia_plan_t* plan = portia_planning_create_plan(planner, 20.0f, 20.0f, 0.0f);

// Add intermediate waypoints
portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.9f);
portia_planning_add_waypoint(planner, plan->id, 10.0f, 10.0f, 0.0f, 0.85f);
portia_planning_add_waypoint(planner, plan->id, 15.0f, 15.0f, 0.0f, 0.8f);

// Execute step by step
while (plan->state != PLAN_STATE_COMPLETE &&
       plan->state != PLAN_STATE_FAILED) {
    portia_planning_evaluate(planner, plan->id);
    portia_planning_execute_step(planner, plan->id);
}
```

### Example 3: Detour Planning

```c
portia_plan_t* plan = portia_planning_create_plan(planner, 15.0f, 15.0f, 0.0f);

// Add invisible waypoints (low confidence)
portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.3f);
portia_planning_add_waypoint(planner, plan->id, 10.0f, 10.0f, 0.0f, 0.4f);

portia_planning_evaluate(planner, plan->id);

if (portia_planning_can_detour(planner, plan->id)) {
    // Proceed with blind navigation
    portia_planning_execute_step(planner, plan->id);
} else {
    // Detour not feasible
    printf("Cannot proceed - too many invisible waypoints\n");
}
```

### Example 4: Obstacle Handling

```c
portia_plan_t* plan = portia_planning_create_plan(planner, 20.0f, 20.0f, 0.0f);
portia_planning_add_waypoint(planner, plan->id, 10.0f, 10.0f, 0.0f, 0.9f);

// Execute until obstacle
portia_planning_execute_step(planner, plan->id);

// Encounter obstacle
bool handled = portia_planning_handle_obstacle(
    planner, plan->id, 12.0f, 12.0f, 0.0f);

if (handled) {
    // Backtracked successfully
    printf("Backtracked to waypoint %u\n", plan->current_waypoint);
} else {
    // Plan failed
    printf("Cannot avoid obstacle - plan failed\n");
}
```

## Security Features

### BBB (Blood-Brain Barrier) Integration

All functions validate inputs using BBB helpers:

```c
// Pointer validation
if (!bbb_validate_pointer(NULL, planner, "function_name")) {
    return error;
}

// Range validation
if (!bbb_validate_range(value, min, max, "function_name")) {
    return error;
}

// Audit logging
bbb_audit_log(BBB_AUDIT_INFO, "portia_planning", "event_name",
              "details");
```

### Memory Safety

- Uses `nimcp_malloc()`, `nimcp_calloc()`, `nimcp_free()` (NOT unified)
- All allocations checked for failure
- Proper cleanup on error paths
- Thread-safe reference counting

### Logging Standards

- Uses `LOG_DEBUG()`, `LOG_INFO()`, `LOG_WARN()`, `LOG_ERROR()`
- NOT `NIMCP_LOG_*` macros
- Module identifier: `"portia_planning"`

## Performance Characteristics

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| Plan creation | O(1) | Fixed allocation |
| Add waypoint | O(1) | Array append |
| Evaluate plan | O(n) | n = waypoint count |
| Execute step | O(1) | State update |
| Find plan | O(m) | m = plan count (linear search) |
| Delete plan | O(1) | Direct access |

Memory usage:
- Planner: ~200 bytes + (max_plans * sizeof(portia_plan_t))
- Plan: ~64 bytes + (max_waypoints * sizeof(plan_waypoint_t))
- Total: ~200 bytes + (4 * 64) + (4 * 16 * 32) = ~2.5 KB (default config)

## Testing

### Unit Tests

Location: `/test/unit/portia/test_portia_planning.cpp`

Test coverage:
- ✓ Initialization with valid/invalid configs
- ✓ Plan creation and deletion
- ✓ Waypoint management
- ✓ Plan evaluation
- ✓ Step execution
- ✓ Detour feasibility
- ✓ Obstacle handling
- ✓ Confidence decay
- ✓ Memory constraints
- ✓ Error handling

Run tests:
```bash
cd build
ctest -R test_portia_planning -V
```

### Demo Program

Location: `/examples/portia_planning_demo.c`

Demonstrates:
1. Simple direct path planning
2. Multi-waypoint route with confidence decay
3. Detour planning with limited visibility
4. Obstacle handling with backtracking
5. Memory-constrained planning

Run demo:
```bash
cd build
./examples/portia_planning_demo
```

## Integration with NIMCP

### Bio-Async Events

The planning system broadcasts events on the SEROTONIN channel:

```c
// Plan lifecycle events
BIO_MSG_PLAN_CREATED
BIO_MSG_PLAN_COMPLETED
BIO_MSG_PLAN_FAILED
BIO_MSG_PLAN_BACKTRACKED
```

### Cognitive Integration

Can be integrated with:
- **Attention system**: Prioritize plan evaluation
- **Working memory**: Store active plan state
- **Curiosity**: Explore alternative routes
- **Executive function**: High-level goal management

## Design Patterns

### 1. State Machine
Plans transition through well-defined states:
```
IDLE → SCANNING → EVALUATING → EXECUTING → COMPLETE
                       ↓              ↓
                   DETOUR ← ← ← ← ← ↓
                       ↓              ↓
                   FAILED ← ← ← ← ← ↓
```

### 2. Confidence Decay
Exponential decay models biological memory:
- Recent observations: high confidence
- Old observations: low confidence
- Very old: effectively forgotten

### 3. Resource Constraints
Fixed memory limits force efficient planning:
- Maximum waypoints per plan
- Maximum concurrent plans
- Maximum detour depth

### 4. Opportunistic Replanning
Re-evaluation can trigger:
- Better route discovery
- Obstacle avoidance
- Confidence updates

## Future Enhancements

### Potential Additions

1. **A* Pathfinding**: Optimal route calculation
2. **Dynamic Obstacles**: Moving obstacle handling
3. **Multi-Agent Planning**: Coordinate multiple planners
4. **Learning**: Learn from past successes/failures
5. **Visualization**: Real-time plan visualization
6. **Cost Functions**: Customizable cost models
7. **3D Terrain**: Height-aware planning
8. **Risk Assessment**: Factor uncertainty into planning

### Extension Points

The system is designed for extension:
- Custom confidence decay functions
- Alternative cost calculations
- Pluggable evaluation strategies
- Bio-async event handlers

## References

### Portia Spider Research

1. Harland, D. P., & Jackson, R. R. (2000). "Eight-legged cats" and how they see - a review of recent research on jumping spiders.
2. Cross, F. R., & Jackson, R. R. (2016). The execution of planned detours by spider-eating predators.
3. Tarsitano, M. S., & Jackson, R. R. (1997). Araneophagic jumping spiders discriminate between detour routes that do and do not lead to prey.

### NIMCP Documentation

- [Bio-Async System](./BIO_ASYNC_INTEGRATION_SUMMARY.md)
- [BBB Security](./BBB_INTEGRATION_COMPLETE_SUMMARY.md)
- [Cognitive Architecture](./COGNITIVE_INTEGRATION_HUB_SUMMARY.md)

## License

Part of the NIMCP (Neuromorphic Inspired Massively Parallel Computing) project.

## Authors

NIMCP Development Team
Date: 2025-12-08

---

*"In the spider's eye, a tiny brain solves problems that challenge our largest computers."*
