# Global Workspace Integration with Executive and Working Memory

## Overview

This document describes the complete integration of the Global Workspace Theory with Executive Function and Working Memory modules in NIMCP.

## Architecture

```
┌──────────────┐       ┌─────────────────┐       ┌────────────────┐
│   Executive  │◄─────►│ Global Workspace│◄─────►│ Working Memory │
│  Controller  │       │                 │       │                │
└──────────────┘       └─────────────────┘       └────────────────┘
       │                        │                         │
       │                        │                         │
       ▼                        ▼                         ▼
┌──────────────────────────────────────────────────────────────────┐
│                      Bio-Async Router                             │
│                                                                   │
│  - BIO_MSG_DECISION_RESPONSE   (Executive broadcasts)            │
│  - BIO_MSG_ATTENTION_SHIFT     (Workspace ignition)              │
│  - BIO_MSG_WORKING_MEMORY_STORE (WM→Workspace)                   │
└──────────────────────────────────────────────────────────────────┘
```

## Component Changes

### 1. Executive Controller (`nimcp_executive.c`)

#### Struct Additions
```c
struct executive_controller {
    // ... existing fields ...

    // Global Workspace integration
    global_workspace_t* workspace;           // Workspace handle
    bool workspace_integration_enabled;       // Integration active flag
    float workspace_ignition_threshold;       // Threshold for broadcasting (0.7)
};
```

#### New Functions
- `executive_set_workspace(exec, workspace)` - Associate exec with workspace
- `broadcast_decision_to_workspace(exec, task, confidence)` - Broadcast significant decisions
- `handle_workspace_ignition(msg, ...)` - Handle workspace broadcasts via bio-async

#### Integration Points
- **Task Completion**: When `executive_complete_task()` completes high-priority tasks with sufficient confidence, decision is broadcast to workspace
- **Workspace Attention**: Executive subscribes to `BIO_MSG_ATTENTION_SHIFT` to receive workspace ignition notifications
- **Bio-Async**: Executive sends `BIO_MSG_DECISION_RESPONSE` when decisions win workspace competition

### 2. Working Memory (`nimcp_working_memory.c`)

#### Struct Additions
```c
struct working_memory {
    // ... existing fields ...

    // Global Workspace integration
    global_workspace_t* workspace;           // Workspace handle
    bool workspace_integration_enabled;       // Integration active flag
    float workspace_salience_threshold;       // Threshold for ignition (0.8)
};
```

#### New Functions
- `working_memory_set_workspace(wm, workspace)` - Associate WM with workspace
- `trigger_workspace_ignition_for_item(wm, index)` - Send salient item to workspace
- `handle_workspace_broadcast(msg, ...)` - Handle workspace content for WM refresh

#### Integration Points
- **Salient Item Addition**: When `working_memory_add()` adds item with salience above threshold, compete for workspace access
- **Workspace Refresh**: When workspace broadcasts content, refresh related WM items to prevent decay
- **Bio-Async**: WM subscribes to `BIO_MSG_ATTENTION_SHIFT` for workspace broadcasts

## Behavioral Flows

### Flow 1: Salient WM Item → Workspace → Executive Attention

```
1. working_memory_add(wm, item, size, 0.9)  // High salience item
2. trigger_workspace_ignition_for_item(wm, 0)
3. global_workspace_compete(workspace, MODULE_WORKING_MEMORY, item, size, 0.9)
4. [IF win] global_workspace_broadcast()
5. Bio-async sends BIO_MSG_ATTENTION_SHIFT to all subscribers
6. Executive receives message in handle_workspace_ignition()
7. Executive reads broadcast: global_workspace_read_broadcast()
8. Executive integrates workspace content into decision context
```

### Flow 2: Executive Decision → Workspace → WM Refresh

```
1. executive_complete_task(exec, true, time)  // Complete high-priority task
2. broadcast_decision_to_workspace(exec, task, 0.8)
3. global_workspace_compete(workspace, MODULE_EXECUTIVE, decision_vec, 256, 0.8)
4. [IF win] global_workspace_broadcast()
5. Bio-async sends BIO_MSG_ATTENTION_SHIFT + BIO_MSG_DECISION_RESPONSE
6. WM receives BIO_MSG_ATTENTION_SHIFT in handle_workspace_broadcast()
7. WM reads broadcast content
8. WM refreshes items related to decision (prevents decay)
```

### Flow 3: Threshold-Based Filtering

```
Executive Decision Confidence:
- SUCCESS + PRIORITY_HIGH: confidence = 0.9 ✓ broadcast
- SUCCESS + PRIORITY_NORMAL: confidence = 0.8 ✓ broadcast
- SUCCESS + PRIORITY_LOW: confidence = 0.8 ✓ broadcast
- FAILURE + PRIORITY_HIGH: confidence = 0.6 ✗ no broadcast
- FAILURE + PRIORITY_NORMAL: confidence = 0.5 ✗ no broadcast

Working Memory Item Salience:
- salience >= 0.8: compete for workspace ✓
- salience < 0.8: local only ✗
```

## Security Integration

All workspace interactions are logged via Blood-Brain Barrier:

```c
// Executive side
LOG_INFO("Executive decision broadcast to workspace: task=%s, confidence=%.2f",
         task->name, confidence);

// Working Memory side
LOG_INFO("WM item triggered workspace ignition: index=%u, salience=%.2f",
         index, salience);

// Workspace ignition received
LOG_DEBUG("Executive received workspace ignition: id=%u, strength=%.2f",
          ignition->target_id, ignition->attention_weight);
```

## Configuration

### Executive Configuration
```c
executive_config_t config = executive_default_config();
executive_controller_t* exec = executive_create_custom(&config);
executive_set_workspace(exec, workspace);
// Sets: workspace_ignition_threshold = 0.7
```

### Working Memory Configuration
```c
working_memory_config_t config = working_memory_default_config();
working_memory_t* wm = working_memory_create_custom(&config);
working_memory_set_workspace(wm, workspace);
// Sets: workspace_salience_threshold = 0.8
```

## Testing Strategy

### Unit Tests
1. **Executive-Workspace Integration** (`test_executive_workspace_integration.c`)
   - Workspace association
   - High-priority decision broadcast
   - Low-priority no broadcast
   - Failed task lower confidence
   - Workspace ignition handler
   - Multiple task cycles
   - NULL workspace safety

2. **Working Memory-Workspace Integration** (`test_working_memory_workspace_integration.c`)
   - Workspace association
   - Salient item ignition
   - Below-threshold no ignition
   - Workspace broadcast refresh
   - Multiple item cycles
   - NULL workspace safety

### Integration Tests
3. **Workspace-Mediated Flow** (`test_workspace_mediated_flow.c`)
   - WM→Workspace→Executive flow
   - Executive→Workspace→WM flow
   - Competition and ignition
   - Bio-async message delivery
   - Multi-module coordination

### Regression Tests
4. **Performance Validation** (`test_workspace_integration_performance.c`)
   - Workspace overhead measurement
   - Bio-async latency impact
   - Memory usage validation
   - Throughput under load

## API Summary

### Executive Functions
```c
void executive_set_workspace(executive_controller_t* exec, global_workspace_t* workspace);
// Internal: static bool broadcast_decision_to_workspace(exec, task, confidence);
// Internal: static nimcp_error_t handle_workspace_ignition(msg, ...);
```

### Working Memory Functions
```c
void working_memory_set_workspace(working_memory_t* wm, global_workspace_t* workspace);
// Internal: static bool trigger_workspace_ignition_for_item(wm, index);
// Internal: static nimcp_error_t handle_workspace_broadcast(msg, ...);
```

### Bio-Async Message Types
- `BIO_MSG_DECISION_RESPONSE` - Executive decision broadcast
- `BIO_MSG_ATTENTION_SHIFT` - Workspace ignition notification
- `BIO_MSG_WORKING_MEMORY_STORE` - WM item storage request

## Compilation Units

Files modified:
1. `/home/bbrelin/nimcp/include/cognitive/nimcp_executive.h` - Add `executive_set_workspace()` API
2. `/home/bbrelin/nimcp/src/cognitive/executive/nimcp_executive.c` - Implement workspace integration
3. `/home/bbrelin/nimcp/include/cognitive/nimcp_working_memory.h` - Add `working_memory_set_workspace()` API
4. `/home/bbrelin/nimcp/src/cognitive/working_memory/nimcp_working_memory.c` - Implement workspace integration

Files created:
1. `/home/bbrelin/nimcp/test/unit/cognitive/test_executive_workspace_integration.c`
2. `/home/bbrelin/nimcp/test/unit/cognitive/test_working_memory_workspace_integration.c`
3. `/home/bbrelin/nimcp/test/integration/test_workspace_mediated_flow.c`
4. `/home/bbrelin/nimcp/test/regression/test_workspace_integration_performance.c`

## Dependencies

### Header Dependencies
```c
#include "cognitive/global_workspace/nimcp_global_workspace.h"  // Workspace API
#include "async/nimcp_bio_messages.h"                            // Bio-async messages
#include "utils/logging/nimcp_logging.h"                        // Logging
```

### Link Dependencies
- `nimcp_global_workspace` library
- `nimcp_bio_async` library
- `nimcp_logging` library

## Performance Characteristics

### Executive Workspace Integration
- **Decision Broadcast Overhead**: O(1) - constant time to encode decision vector
- **Workspace Competition**: O(log n) where n = number of competing modules
- **Ignition Handling**: O(1) - read broadcast and log

### Working Memory Workspace Integration
- **Item Addition Overhead**: +O(1) if above threshold (competition check)
- **Workspace Refresh**: O(1) per item refresh
- **Broadcast Handling**: O(k) where k = items related to broadcast

### Memory Overhead
- Executive: +24 bytes (workspace pointer + flags)
- Working Memory: +24 bytes (workspace pointer + flags)
- Bio-async messages: ~512 bytes per message in flight

## Future Enhancements

1. **Adaptive Thresholds**: Dynamically adjust ignition thresholds based on cognitive load
2. **Workspace History**: Maintain short-term history of workspace contents for temporal reasoning
3. **Multi-Workspace**: Support multiple workspace contexts (task-specific workspaces)
4. **Conscious Reporting**: Generate natural language descriptions of workspace contents
5. **Metacognitive Monitoring**: Track what reaches consciousness vs. what remains unconscious

## References

- Baars, B. J. (1988). A Cognitive Theory of Consciousness
- Dehaene, S., & Changeux, J. P. (2011). Experimental and theoretical approaches to conscious processing
- Miller, G. A. (1956). The magical number seven, plus or minus two
