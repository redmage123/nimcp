# Portia Planning System - Quick Reference

## One-Minute Overview

Memory-constrained route planning inspired by Portia spiders. Plans complex detours with limited waypoints, confidence decay, and backtracking.

## Quick Start

```c
#include "portia/nimcp_portia_planning.h"

// 1. Initialize
portia_planning_config_t config = {
    .max_waypoints = 16,
    .max_plans = 4,
    .max_detour_depth = 3,
    .scan_interval_s = 0.1f,
    .confidence_threshold = 0.6f,
    .enable_backtracking = true
};
portia_planner_t planner = portia_planning_init(&config, NULL);

// 2. Create plan
portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);

// 3. Add waypoints
portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.9f);

// 4. Execute
while (plan->state != PLAN_STATE_COMPLETE && plan->state != PLAN_STATE_FAILED) {
    portia_planning_evaluate(planner, plan->id);
    portia_planning_execute_step(planner, plan->id);
}

// 5. Cleanup
portia_planning_delete_plan(planner, plan->id);
portia_planning_destroy(planner);
```

## Core Functions

| Function | Purpose | Returns |
|----------|---------|---------|
| `portia_planning_init()` | Create planner | `portia_planner_t` or NULL |
| `portia_planning_destroy()` | Free planner | void |
| `portia_planning_create_plan()` | Start new plan | `portia_plan_t*` or NULL |
| `portia_planning_add_waypoint()` | Add waypoint | bool |
| `portia_planning_evaluate()` | Re-evaluate plan | bool |
| `portia_planning_execute_step()` | Advance plan | bool |
| `portia_planning_handle_obstacle()` | Replan around obstacle | bool |
| `portia_planning_can_detour()` | Check detour feasibility | bool |
| `portia_planning_get_state()` | Query plan state | `plan_state_t` |
| `portia_planning_get_plan()` | Get plan by ID | `portia_plan_t*` or NULL |
| `portia_planning_delete_plan()` | Remove plan | bool |

## Plan States

```
IDLE → SCANNING → EVALUATING → EXECUTING → COMPLETE
                       ↓              ↓
                   DETOUR ← ← ← ← ← ↓
                       ↓              ↓
                   FAILED ← ← ← ← ← ↓
```

## Configuration

```c
typedef struct {
    uint32_t max_waypoints;      // Memory limit (default: 16)
    uint32_t max_plans;          // Concurrent plans (default: 4)
    uint32_t max_detour_depth;   // Blind navigation limit (default: 3)
    float scan_interval_s;       // Re-eval interval (default: 0.1s)
    float confidence_threshold;  // Min confidence (default: 0.6)
    bool enable_backtracking;    // Allow backtrack (default: true)
} portia_planning_config_t;
```

## Waypoint Structure

```c
typedef struct {
    float x, y, z;              // Position
    float confidence;           // 0.0-1.0
    uint64_t last_seen_ms;      // Timestamp
    bool visible;               // Above threshold
} plan_waypoint_t;
```

## Common Patterns

### Simple Path
```c
portia_plan_t* plan = portia_planning_create_plan(planner, x, y, z);
portia_planning_execute_step(planner, plan->id);
```

### Multi-Waypoint Route
```c
portia_plan_t* plan = portia_planning_create_plan(planner, 20, 20, 0);
portia_planning_add_waypoint(planner, plan->id, 5, 5, 0, 0.9f);
portia_planning_add_waypoint(planner, plan->id, 10, 10, 0, 0.85f);
portia_planning_add_waypoint(planner, plan->id, 15, 15, 0, 0.8f);

while (plan->state != PLAN_STATE_COMPLETE) {
    portia_planning_evaluate(planner, plan->id);
    portia_planning_execute_step(planner, plan->id);
}
```

### Obstacle Handling
```c
// During execution
if (obstacle_detected) {
    bool handled = portia_planning_handle_obstacle(
        planner, plan->id, obs_x, obs_y, obs_z);

    if (!handled) {
        // Plan failed - no alternatives
    }
}
```

### Detour Check
```c
if (!portia_planning_can_detour(planner, plan->id)) {
    // Too many invisible waypoints
    // Consider replanning or failing
}
```

## Key Behaviors

### Confidence Decay
- Exponential decay: `conf(t) = conf(0) * exp(-0.693 * t)`
- Half-life: 1 second
- Updates on `portia_planning_evaluate()`

### Detour Planning
- Can navigate through invisible waypoints
- Limited by `max_detour_depth`
- State changes to `PLAN_STATE_DETOUR`

### Backtracking
- Returns to previous waypoint on obstacle
- Only if `enable_backtracking = true`
- Fails if already at start

### Memory Constraints
- `max_waypoints` per plan enforced
- `max_plans` total enforced
- Mimics spider's limited working memory

## Error Handling

```c
if (!result) {
    const char* error = portia_planning_get_last_error();
    LOG_ERROR("Planning error: %s", error);
}
```

## Security Standards

```c
// ✓ Use nimcp_malloc/calloc/free
void* mem = nimcp_malloc(size);

// ✓ Use LOG_DEBUG/INFO/WARN/ERROR
LOG_INFO("Plan created: id=%u", plan->id);

// ✓ Validate pointers
if (!bbb_validate_pointer(NULL, planner, "func_name")) return false;

// ✓ Validate ranges
if (!bbb_validate_range(value, min, max, "func_name")) return false;

// ✓ Audit log security events
bbb_audit_log(BBB_AUDIT_INFO, "portia_planning", "event", "details");
```

## Performance

| Operation | Complexity | Typical Time |
|-----------|-----------|--------------|
| Create plan | O(1) | ~10 μs |
| Add waypoint | O(1) | ~5 μs |
| Evaluate | O(n) | ~50 μs (n=16) |
| Execute step | O(1) | ~5 μs |
| Delete plan | O(1) | ~10 μs |

Memory: ~2.5 KB (default config)

## Testing

```bash
# Unit tests
cd build
ctest -R test_portia_planning -V

# Demo
./examples/portia_planning_demo
```

## Files

```
include/portia/nimcp_portia_planning.h     # Header
src/portia/nimcp_portia_planning.c         # Implementation
test/unit/portia/test_portia_planning.cpp  # Tests
examples/portia_planning_demo.c            # Demo
```

## Common Pitfalls

### ❌ Don't
```c
// Don't use unified memory
void* ptr = nimcp_unified_malloc(size);  // WRONG

// Don't use old logging
NIMCP_LOG_INFO("...");  // WRONG

// Don't skip validation
if (!planner) return false;  // Incomplete
```

### ✓ Do
```c
// Use standard memory
void* ptr = nimcp_malloc(size);  // CORRECT

// Use new logging
LOG_INFO("...");  // CORRECT

// Full validation
if (!bbb_validate_pointer(NULL, planner, "func")) return false;  // CORRECT
```

## Bio-Async Integration

```c
// Initialize with bio-async context
portia_planner_t planner = portia_planning_init(&config, bio_ctx);

// Events broadcast on SEROTONIN channel:
// - plan_created
// - plan_completed
// - plan_failed
```

## Quick Debug

```c
// Check plan state
printf("State: %d\n", plan->state);
printf("Current waypoint: %u/%u\n", plan->current_waypoint, plan->waypoint_count);
printf("Progress: %.1f%%\n", plan->progress * 100.0f);

// Check waypoint details
for (uint32_t i = 0; i < plan->waypoint_count; i++) {
    plan_waypoint_t* wp = &plan->waypoints[i];
    printf("  [%u] (%.1f, %.1f, %.1f) conf=%.2f vis=%s\n",
           i, wp->x, wp->y, wp->z, wp->confidence,
           wp->visible ? "YES" : "NO");
}
```

## Links

- [Full Documentation](./PORTIA_PLANNING_SYSTEM.md)
- [Bio-Async System](./BIO_ASYNC_INTEGRATION_SUMMARY.md)
- [BBB Security](./BBB_INTEGRATION_COMPLETE_SUMMARY.md)

---

**Remember**: Portia spiders plan complex routes with a brain smaller than a pinhead. Our system honors that elegant simplicity with memory-constrained, confidence-based planning.
