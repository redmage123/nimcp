# Multi-Swarm Coordination Quick Reference

## Quick Start

```c
// 1. Create coordinator
nimcp_multi_swarm_coordinator_t* coordinator =
    nimcp_multi_swarm_create(brain, router);

// 2. Create super-swarm
nimcp_super_swarm_t* super =
    nimcp_super_swarm_create(coordinator, "Alpha");

// 3. Create and register swarms
nimcp_swarm_identity_t* swarm =
    nimcp_swarm_identity_create(coordinator, "Recon-1", 20);
nimcp_swarm_register(coordinator, swarm);
nimcp_super_swarm_add_swarm(super, swarm);

// 4. Add capabilities
nimcp_swarm_add_capability(swarm,
    NIMCP_SWARM_CAP_RECONNAISSANCE, 0.9f, 20, true);

// 5. Set territory
nimcp_coord3d_t min = {0, 0, 0};
nimcp_coord3d_t max = {100, 100, 50};
nimcp_swarm_set_territory(swarm, min, max, true, 1.0f);

// 6. Create mission
uint64_t mission = nimcp_mission_create(coordinator,
    "Reconnaissance mission",
    NIMCP_MISSION_PRIORITY_HIGH,
    operation_area, deadline);

// 7. Assign swarms
uint64_t swarms[] = {swarm->swarm_id};
nimcp_mission_assign_swarms(coordinator, mission, swarms, 1);

// 8. Update progress
nimcp_mission_update_progress(coordinator, mission, 0.5f);

// 9. Complete mission
nimcp_mission_complete(coordinator, mission, true);

// 10. Cleanup
nimcp_multi_swarm_destroy(coordinator);
```

## Core Data Structures

### Swarm Identity
```c
typedef struct {
    uint64_t swarm_id;                    // Unique ID
    char name[64];                        // Swarm name
    uint32_t agent_count;                 // Total agents
    uint32_t active_agents;               // Active agents
    nimcp_swarm_health_t health;          // Health status
    float health_percentage;              // Health (0-1)
    nimcp_swarm_capability_t capabilities[32];
    uint32_t capability_count;
    nimcp_territory_bounds_t territory;   // Territory
} nimcp_swarm_identity_t;
```

### Super-Swarm
```c
typedef struct {
    uint64_t super_swarm_id;
    char name[64];
    nimcp_swarm_identity_t* swarms[64];
    uint32_t swarm_count;
    nimcp_mission_assignment_t missions[16];
    nimcp_comm_bridge_t bridges[8];
} nimcp_super_swarm_t;
```

### Multi-Swarm Coordinator
```c
typedef struct {
    nimcp_super_swarm_t* super_swarms[64];
    uint32_t super_swarm_count;
    nimcp_hash_table_t* swarm_registry;
    nimcp_hash_table_t* mission_registry;
    nimcp_brain_t* brain;
    nimcp_bio_router_t* router;
} nimcp_multi_swarm_coordinator_t;
```

## Capabilities

```c
NIMCP_SWARM_CAP_SURVEILLANCE      // Surveillance
NIMCP_SWARM_CAP_TRANSPORT         // Transport
NIMCP_SWARM_CAP_COMBAT            // Combat
NIMCP_SWARM_CAP_RESCUE            // Search & rescue
NIMCP_SWARM_CAP_CONSTRUCTION      // Construction
NIMCP_SWARM_CAP_MEDICAL           // Medical support
NIMCP_SWARM_CAP_COMMUNICATION     // Communication relay
NIMCP_SWARM_CAP_RECONNAISSANCE    // Reconnaissance
NIMCP_SWARM_CAP_DEFENSE           // Defense
NIMCP_SWARM_CAP_LOGISTICS         // Logistics
```

## Health Status

```c
NIMCP_SWARM_HEALTH_EXCELLENT      // > 90% operational
NIMCP_SWARM_HEALTH_GOOD           // 70-90% operational
NIMCP_SWARM_HEALTH_FAIR           // 50-70% operational
NIMCP_SWARM_HEALTH_POOR           // 30-50% operational
NIMCP_SWARM_HEALTH_CRITICAL       // < 30% operational
```

## Mission Priorities

```c
NIMCP_MISSION_PRIORITY_CRITICAL   // Life-threatening
NIMCP_MISSION_PRIORITY_HIGH       // Urgent
NIMCP_MISSION_PRIORITY_MEDIUM     // Normal
NIMCP_MISSION_PRIORITY_LOW        // Background
NIMCP_MISSION_PRIORITY_IDLE       // Maintenance
```

## Mission Status

```c
NIMCP_MISSION_STATUS_PENDING      // Awaiting assignment
NIMCP_MISSION_STATUS_ASSIGNED     // Assigned to swarms
NIMCP_MISSION_STATUS_ACTIVE       // Currently executing
NIMCP_MISSION_STATUS_PAUSED       // Temporarily paused
NIMCP_MISSION_STATUS_COMPLETED    // Successfully completed
NIMCP_MISSION_STATUS_FAILED       // Mission failed
NIMCP_MISSION_STATUS_ABORTED      // Mission aborted
```

## Resource Request Types

```c
NIMCP_RESOURCE_REQ_DRONES         // Request drones
NIMCP_RESOURCE_REQ_ENERGY         // Request energy
NIMCP_RESOURCE_REQ_INFORMATION    // Request information
NIMCP_RESOURCE_REQ_CAPABILITY     // Request capability
NIMCP_RESOURCE_REQ_COORDINATION   // Request coordination
NIMCP_RESOURCE_REQ_TERRITORY      // Request territory access
```

## Conflict Resolution Strategies

```c
NIMCP_CONFLICT_PRIORITY           // Higher priority wins
NIMCP_CONFLICT_NEGOTIATION        // Negotiate solution
NIMCP_CONFLICT_TIME_SHARING       // Share over time
NIMCP_CONFLICT_SPATIAL_SHARING    // Divide space
NIMCP_CONFLICT_COOPERATION        // Cooperate
NIMCP_CONFLICT_ESCALATION         // Escalate to super-swarm
```

## Common Operations

### Swarm Management
```c
// Create swarm
nimcp_swarm_identity_t* swarm =
    nimcp_swarm_identity_create(coord, name, agent_count);

// Register swarm
nimcp_swarm_register(coordinator, swarm);

// Add capability
nimcp_swarm_add_capability(swarm, type, proficiency,
                          capacity, is_lendable);

// Update health
nimcp_swarm_update_health(swarm, active_agents);

// Set territory
nimcp_swarm_set_territory(swarm, min, max,
                         is_dynamic, priority);
```

### Territory Management
```c
// Check overlap
bool overlaps = nimcp_territory_overlaps(bounds_a, bounds_b);

// Negotiate
nimcp_territory_negotiate(coordinator, swarm_a, swarm_b);

// Detect conflicts
uint32_t count = nimcp_territory_detect_conflicts(
    coordinator, conflicts_vector);
```

### Resource Sharing
```c
// Request resources
uint64_t req_id = nimcp_resource_request(coordinator,
    requesting_swarm, target_swarm, type,
    quantity, priority);

// Approve request
nimcp_resource_approve(coordinator, req_id, cost);

// Deny request
nimcp_resource_deny(coordinator, req_id);

// Process requests
uint32_t processed = nimcp_resource_process_requests(
    coordinator, allocator_callback, user_data);
```

### Mission Management
```c
// Create mission
uint64_t mission = nimcp_mission_create(coordinator,
    description, priority, area, deadline);

// Assign swarms
uint64_t swarms[] = {id1, id2, id3};
nimcp_mission_assign_swarms(coordinator, mission,
                           swarms, 3);

// Update progress (0.0 to 1.0)
nimcp_mission_update_progress(coordinator, mission, 0.75f);

// Complete mission
nimcp_mission_complete(coordinator, mission, success);

// Get mission
nimcp_mission_assignment_t* m =
    nimcp_mission_get(coordinator, mission);
```

### Communication Bridges
```c
// Create bridge
uint64_t bridge = nimcp_comm_bridge_create(coordinator,
    swarm_a, swarm_b, relay_agents, relay_count);

// Update quality (0.0 to 1.0)
nimcp_comm_bridge_update_quality(coordinator,
                                bridge, 0.85f);

// Route message
nimcp_comm_bridge_route_message(coordinator,
    from_swarm, to_swarm, message);

// Deactivate bridge
nimcp_comm_bridge_deactivate(coordinator, bridge);
```

### Conflict Resolution
```c
// Detect conflicts
uint32_t conflicts = nimcp_conflict_detect(coordinator);

// Resolve specific conflict
nimcp_conflict_resolve(coordinator, conflict_id,
                      strategy, resolver_callback, user_data);

// Auto-resolve all
uint32_t resolved = nimcp_conflict_auto_resolve(
    coordinator, resolver_callback, user_data);
```

### Bio-Async Integration
```c
// Process inbox messages
uint32_t processed =
    nimcp_multi_swarm_process_inbox(coordinator);

// Broadcast discovery
nimcp_multi_swarm_broadcast_discovery(coordinator,
                                     swarm_id);

// Send message
nimcp_multi_swarm_send_message(coordinator,
    from_swarm, to_swarm, msg_type,
    payload, payload_size);
```

### Queries
```c
// Get swarm by ID
nimcp_swarm_identity_t* swarm =
    nimcp_swarm_get(coordinator, swarm_id);

// Find by capability
uint32_t found = nimcp_swarm_find_by_capability(
    coordinator, capability, min_proficiency, results);

// Find in territory
uint32_t found = nimcp_swarm_find_in_territory(
    coordinator, territory, results);

// Get statistics
uint32_t total_swarms, total_agents,
         active_missions, active_conflicts;
nimcp_multi_swarm_get_stats(coordinator,
    &total_swarms, &total_agents,
    &active_missions, &active_conflicts);

// Print status
nimcp_multi_swarm_print_status(coordinator, verbose);
```

## Configuration Flags

```c
coordinator->enable_auto_negotiation = true;  // Auto-negotiate
coordinator->enable_resource_sharing = true;  // Allow sharing
coordinator->enable_bridge_formation = true;  // Auto bridges
```

## Constants

```c
NIMCP_MAX_SWARMS_PER_SUPER       64   // Max swarms
NIMCP_MAX_SWARM_CAPABILITIES     32   // Max capabilities
NIMCP_MAX_SWARM_MISSIONS         16   // Max missions
NIMCP_MAX_COMM_BRIDGES           8    // Max bridges
NIMCP_SWARM_NAME_MAX             64   // Max name length
```

## Message Types (Bio-Async)

```c
MSG_TYPE_SWARM_DISCOVERY       0x1001  // Discovery
MSG_TYPE_RESOURCE_REQUEST      0x1002  // Resource req
MSG_TYPE_RESOURCE_RESPONSE     0x1003  // Resource resp
MSG_TYPE_TERRITORY_NEGOTIATE   0x1004  // Territory neg
MSG_TYPE_MISSION_UPDATE        0x1005  // Mission update
MSG_TYPE_CONFLICT_ALERT        0x1006  // Conflict alert
```

## Error Handling

All functions return `nimcp_result_t` or appropriate types:

```c
nimcp_result_t result = nimcp_swarm_register(coord, swarm);
if (result != NIMCP_OK) {
    // Handle error
    NIMCP_LOG_ERROR("Failed to register swarm: %d", result);
}
```

Common return codes:
- `NIMCP_OK` - Success
- `NIMCP_ERROR_INVALID_PARAMETER` - Invalid parameter
- `NIMCP_ERROR_NOT_FOUND` - Item not found
- `NIMCP_ERROR_OUT_OF_MEMORY` - Allocation failed
- `NIMCP_ERROR_OUT_OF_RANGE` - Exceeded limits
- `NIMCP_ERROR_ALREADY_EXISTS` - Item exists
- `NIMCP_ERROR_NOT_INITIALIZED` - Not initialized

## Thread Safety

All public APIs are thread-safe using read-write locks:

```c
// Multiple readers can access simultaneously
nimcp_swarm_identity_t* swarm =
    nimcp_swarm_get(coordinator, id);

// Writers have exclusive access
nimcp_swarm_register(coordinator, swarm);
```

## Best Practices

1. **Always check return values**
```c
if (nimcp_swarm_register(coord, swarm) != NIMCP_OK) {
    // Handle error
}
```

2. **Set territories before operations**
```c
nimcp_swarm_set_territory(swarm, min, max, true, 1.0f);
```

3. **Update health regularly**
```c
nimcp_swarm_update_health(swarm, active_count);
```

4. **Process inbox periodically**
```c
nimcp_multi_swarm_process_inbox(coordinator);
```

5. **Monitor conflicts**
```c
nimcp_conflict_detect(coordinator);
nimcp_conflict_auto_resolve(coordinator, NULL, NULL);
```

6. **Clean up properly**
```c
nimcp_multi_swarm_destroy(coordinator);
```

## Performance Tips

- Use super-swarms to organize related swarms
- Enable auto-negotiation for autonomous operation
- Set appropriate bridge quality thresholds
- Use capability queries to find suitable swarms
- Batch resource requests when possible
- Monitor health metrics for proactive management

## Common Patterns

### Hierarchical Organization
```c
nimcp_super_swarm_t* division =
    nimcp_super_swarm_create(coord, "1st Division");
nimcp_super_swarm_t* battalion =
    nimcp_super_swarm_create(coord, "Alpha Battalion");
// Add swarms to appropriate super-swarms
```

### Joint Operations
```c
// Create mission
uint64_t mission = nimcp_mission_create(...);

// Assign multiple swarms
uint64_t swarms[] = {recon_id, combat_id, support_id};
nimcp_mission_assign_swarms(coord, mission, swarms, 3);

// Create bridges for coordination
nimcp_comm_bridge_create(coord, recon_id, combat_id, ...);
```

### Resource Coordination
```c
// Request resources
uint64_t req = nimcp_resource_request(coord,
    requesting_swarm, provider_swarm,
    NIMCP_RESOURCE_REQ_DRONES, 5,
    NIMCP_MISSION_PRIORITY_HIGH);

// Provider reviews and approves
nimcp_resource_approve(coord, req, 0.2f);
```

### Adaptive Territories
```c
// Set dynamic territory
nimcp_swarm_set_territory(swarm, min, max,
                         true,   // is_dynamic
                         0.5f);  // priority

// Auto-negotiation handles conflicts
coordinator->enable_auto_negotiation = true;
```

## See Also

- Multi-Swarm Implementation: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_multi.c`
- Complete Documentation: `/home/bbrelin/nimcp/docs/MULTI_SWARM_COORDINATION_SUMMARY.md`
- Demo Application: `/home/bbrelin/nimcp/examples/multi_swarm_demo.c`
- Header File: `/home/bbrelin/nimcp/include/swarm/nimcp_swarm_multi.h`
