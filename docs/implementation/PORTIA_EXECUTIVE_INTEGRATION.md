# Portia Spider System Integration with Executive Function Module

## Overview

This document describes the integration of the Portia Spider System with the Executive Function module in NIMCP, enabling resource-aware cognitive task management.

## Implementation Status

### ✅ Completed

1. **Header Updates** (`/home/bbrelin/nimcp/include/cognitive/nimcp_executive.h`):
   - Added `enable_portia_integration` to `executive_config_t`
   - Added three new API functions:
     - `executive_get_portia_tier()` - Query current tier
     - `executive_is_resource_aware()` - Check resource-aware mode
     - `executive_get_recommended_plan_depth()` - Get tier-adjusted planning depth

2. **Structure Updates** (`/home/bbrelin/nimcp/src/cognitive/executive/nimcp_executive.c`):
   - Added Portia includes:
     ```c
     #include "portia/nimcp_portia.h"
     #include "portia/nimcp_portia_messages.h"
     #include "utils/platform/nimcp_platform_tier.h"
     ```
   - Extended `struct executive_controller` with:
     ```c
     // Portia integration (Phase 11.5)
     platform_tier_t current_tier;
     portia_degradation_level_t degradation_level;
     bool resource_aware_mode;
     uint64_t last_tier_change_ms;
     uint32_t tier_change_count;
     ```

3. **Bio-Async Message Handlers**:
   - `handle_portia_tier_change()` - Processes tier transitions
   - `handle_portia_degradation_event()` - Handles degradation events

### 🚧 Remaining Implementation

#### 1. Update `executive_create()` Default Config

**File**: `/home/bbrelin/nimcp/src/cognitive/executive/nimcp_executive.c`
**Line**: ~514-523

**Change**:
```c
executive_config_t default_config = {
    .max_tasks = DEFAULT_MAX_TASKS,
    .task_switch_cost_ms = DEFAULT_SWITCH_COST_MS,
    .inhibition_threshold = DEFAULT_INHIBITION_THRESHOLD,
    .max_plan_depth = DEFAULT_MAX_PLAN_DEPTH,
    .enable_task_prioritization = true,
    .enable_deadline_checking = true,
    .enable_portia_integration = true  // ADD THIS LINE
};
```

#### 2. Initialize Portia Fields in `executive_create_custom()`

**File**: `/home/bbrelin/nimcp/src/cognitive/executive/nimcp_executive.c`
**Location**: After Global Workspace initialization (~610), before Bio-Async registration

**Add**:
```c
// =========================================================================
// PORTIA: Initialize integration fields
// =========================================================================
exec->current_tier = TIER_OPTIMAL;  // Assume optimal until told otherwise
exec->degradation_level = PORTIA_DEGRADATION_NONE;
exec->resource_aware_mode = false;
exec->last_tier_change_ms = 0;
exec->tier_change_count = 0;

// Query current Portia tier if Portia is initialized
if (config->enable_portia_integration && portia_is_initialized()) {
    exec->current_tier = portia_get_current_tier();
    portia_status_t status;
    if (portia_get_status(&status) == NIMCP_SUCCESS) {
        exec->degradation_level = status.degradation_level;
        // Enable resource-aware mode if not in optimal tier
        if (exec->current_tier < TIER_OPTIMAL ||
            exec->degradation_level > PORTIA_DEGRADATION_NONE) {
            exec->resource_aware_mode = true;
        }
    }
    LOG_MODULE_INFO(LOG_MODULE, "Portia integration enabled, tier=%u, degradation=%u",
                   exec->current_tier, exec->degradation_level);
}
```

#### 3. Register Portia Handlers in Bio-Async Section

**File**: `/home/bbrelin/nimcp/src/cognitive/executive/nimcp_executive.c`
**Location**: In bio-async registration block (~628-631)

**Add after existing handler registrations**:
```c
// Register Portia message handlers if integration enabled
if (config->enable_portia_integration) {
    bio_router_register_handler(exec->bio_ctx, BIO_MSG_PORTIA_TIER_CHANGE,
                                handle_portia_tier_change);
    bio_router_register_handler(exec->bio_ctx, BIO_MSG_PORTIA_DEGRADATION_EVENT,
                                handle_portia_degradation_event);
    LOG_MODULE_INFO(LOG_MODULE, "Portia message handlers registered");
}
```

#### 4. Implement API Functions

**File**: `/home/bbrelin/nimcp/src/cognitive/executive/nimcp_executive.c`
**Location**: End of file, before closing

**Add**:
```c
//=============================================================================
// Portia Integration API (Phase 11.5)
//=============================================================================

/**
 * @brief Get current Portia tier from executive controller
 *
 * WHAT: Query Portia tier state
 * WHY:  Allow external monitoring of resource constraints
 * HOW:  Return cached tier value
 *
 * @param exec Executive controller
 * @return Current platform tier, or TIER_UNKNOWN if disabled
 */
uint32_t executive_get_portia_tier(executive_controller_t* exec)
{
    if (!exec || !exec->config.enable_portia_integration) {
        return TIER_UNKNOWN;
    }
    return exec->current_tier;
}

/**
 * @brief Check if executive is in resource-aware mode
 *
 * WHAT: Query resource-aware state
 * WHY:  Diagnostic and status reporting
 * HOW:  Return resource_aware_mode flag
 *
 * @param exec Executive controller
 * @return true if resource-aware mode active
 */
bool executive_is_resource_aware(executive_controller_t* exec)
{
    if (!exec || !exec->config.enable_portia_integration) {
        return false;
    }
    return exec->resource_aware_mode;
}

/**
 * @brief Get recommended max plan depth for current resources
 *
 * WHAT: Query planning depth adjusted for current tier
 * WHY:  Allow external planners to adapt complexity
 * HOW:  Scale max_plan_depth based on current tier and degradation
 *
 * SCALING LOGIC:
 * - TIER_OPTIMAL + NO_DEGRADATION: 100% of max_plan_depth
 * - TIER_MODERATE: 75% of max_plan_depth
 * - TIER_MINIMAL: 50% of max_plan_depth
 * - TIER_DEGRADED/EMERGENCY: 25% of max_plan_depth
 * - Additional reduction based on degradation level
 *
 * @param exec Executive controller
 * @return Recommended max plan depth
 */
uint32_t executive_get_recommended_plan_depth(executive_controller_t* exec)
{
    if (!exec) {
        return DEFAULT_MAX_PLAN_DEPTH;
    }

    uint32_t base_depth = exec->config.max_plan_depth;

    // If Portia integration disabled, return full depth
    if (!exec->config.enable_portia_integration) {
        return base_depth;
    }

    float tier_factor = 1.0f;
    switch (exec->current_tier) {
        case TIER_OPTIMAL:
            tier_factor = 1.0f;
            break;
        case TIER_MODERATE:
            tier_factor = 0.75f;
            break;
        case TIER_MINIMAL:
            tier_factor = 0.5f;
            break;
        case TIER_DEGRADED:
        case TIER_EMERGENCY:
            tier_factor = 0.25f;
            break;
        default:
            tier_factor = 1.0f;
    }

    // Apply degradation penalty
    float degradation_factor = 1.0f;
    switch (exec->degradation_level) {
        case PORTIA_DEGRADATION_NONE:
            degradation_factor = 1.0f;
            break;
        case PORTIA_DEGRADATION_MINOR:
            degradation_factor = 0.9f;
            break;
        case PORTIA_DEGRADATION_MODERATE:
            degradation_factor = 0.75f;
            break;
        case PORTIA_DEGRADATION_SEVERE:
            degradation_factor = 0.5f;
            break;
        case PORTIA_DEGRADATION_EMERGENCY:
            degradation_factor = 0.25f;
            break;
        default:
            degradation_factor = 1.0f;
    }

    // Compute final recommended depth
    float adjusted = (float)base_depth * tier_factor * degradation_factor;
    uint32_t result = (uint32_t)(adjusted + 0.5f);  // Round to nearest

    // Ensure minimum of 1
    return (result < 1) ? 1 : result;
}
```

#### 5. Update `executive_create_plan()` for Resource-Awareness

**File**: `/home/bbrelin/nimcp/src/cognitive/executive/nimcp_executive.c`
**Location**: In `executive_create_plan()` function (~682-745)

**Modify the max_steps validation**:
```c
// Get recommended depth based on current resources
uint32_t recommended_depth = executive_get_recommended_plan_depth(exec);
uint32_t effective_max = (max_steps > recommended_depth) ? recommended_depth : max_steps;

if (max_steps == 0 || max_steps > exec->config.max_plan_depth) {
    set_error("Invalid max_steps: %u (max: %u)", max_steps, exec->config.max_plan_depth);
    return NULL;
}

// Log if resource constraints reduced planning depth
if (exec->resource_aware_mode && effective_max < max_steps) {
    LOG_MODULE_WARN(LOG_MODULE, "Planning depth reduced: %u -> %u (tier=%u, degradation=%u)",
                   max_steps, effective_max, exec->current_tier, exec->degradation_level);
    max_steps = effective_max;
}
```

## Testing Strategy

### Unit Tests

**File**: `/home/bbrelin/nimcp/test/unit/cognitive/executive/test_executive_portia.cpp`

Test cases:
1. `TEST(ExecutivePortia, InitializationWithPortiaEnabled)` - Verify Portia integration initializes correctly
2. `TEST(ExecutivePortia, InitializationWithPortiaDisabled)` - Verify graceful handling when disabled
3. `TEST(ExecutivePortia, TierChangeHandler)` - Test tier change message handling
4. `TEST(ExecutivePortia, DegradationHandler)` - Test degradation event handling
5. `TEST(ExecutivePortia, ResourceAwareModeTrigger)` - Verify resource-aware mode activation
6. `TEST(ExecutivePortia, PlanningDepthScaling)` - Test depth scaling by tier
7. `TEST(ExecutivePortia, QueryFunctions)` - Test API query functions

### Integration Tests

**File**: `/home/bbrelin/nimcp/test/integration/cognitive/executive/test_executive_portia_integration.cpp`

Test scenarios:
1. Full Portia → Executive message flow
2. Tier transitions during active task execution
3. Planning under resource constraints
4. Multiple tier changes in sequence
5. Degradation impact on task queue management

### Regression Tests

**File**: `/home/bbrelin/nimcp/test/regression/cognitive/executive/test_executive_portia_regression.cpp`

Regression scenarios:
1. Executive works without Portia (backward compatibility)
2. Performance impact of Portia integration is minimal
3. No memory leaks in tier change handlers
4. BBB security validation passes

## Security Considerations

1. **BBB Registration**: Executive module already registers with BBB. No additional registration needed.
2. **Message Validation**: All Portia messages validated in handlers (NULL checks, config checks)
3. **Resource Limits**: Planning depth limits prevent unbounded resource consumption
4. **Audit Logging**: Tier changes logged via LOG_MODULE_* macros

## Performance Impact

- **Memory**: +32 bytes per executive_controller (5 new fields)
- **CPU**: Negligible (tier changes are rare events, query functions are O(1))
- **Message Overhead**: 2 additional bio-async handlers (only triggered on Portia events)

## Dependencies

- `portia/nimcp_portia.h` - Main Portia API
- `portia/nimcp_portia_messages.h` - Portia message types
- `utils/platform/nimcp_platform_tier.h` - Platform tier enums

## Usage Example

```c
// Create executive with Portia integration
executive_config_t config = {
    .max_tasks = 16,
    .task_switch_cost_ms = 200.0f,
    .inhibition_threshold = 0.7f,
    .max_plan_depth = 10,
    .enable_task_prioritization = true,
    .enable_deadline_checking = true,
    .enable_portia_integration = true
};

executive_controller_t* exec = executive_create_custom(&config);

// Query current state
uint32_t tier = executive_get_portia_tier(exec);
bool resource_aware = executive_is_resource_aware(exec);
uint32_t recommended_depth = executive_get_recommended_plan_depth(exec);

// Create plan (automatically adapts to resources)
plan_t* plan = executive_create_plan(exec, "Navigate to goal", 10);
// If tier is MINIMAL, actual plan depth may be reduced to 5

// Cleanup
executive_destroy_plan(plan);
executive_destroy(exec);
```

## Next Steps

1. Complete remaining code edits (steps 1-5 above)
2. Write comprehensive test suite
3. Run integration tests with live Portia system
4. Performance profiling
5. Documentation review
