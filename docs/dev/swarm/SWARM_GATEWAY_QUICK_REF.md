# NIMCP Swarm Gateway Quick Reference

## One-Page API Reference

### Include
```c
#include "swarm/nimcp_swarm_gateway.h"
```

### Create & Destroy
```c
// Create
swarm_gateway_config_t config = {
    .gateway_name = "CONTROL",
    .max_swarms = 10,
    .broadcast_interval_ms = 1000,
    .timeout_ms = 5000,
    .enable_learning_sync = true,
    .enable_mission_control = true,
    .enable_telemetry = true
};
swarm_gateway_t* gw = swarm_gateway_create(brain, &config);

// Destroy
swarm_gateway_destroy(gw);
```

### Connect/Disconnect
```c
// Connect to swarm
swarm_gateway_connect_swarm(gw, "ALPHA", "192.168.1.100:8000");

// Disconnect
swarm_gateway_disconnect_swarm(gw, "ALPHA");
```

### Send Messages
```c
// Mission
mission_params_t mission = {
    .mission_id = "RECON_001",
    .target_coordinates = {100.0f, 200.0f, 50.0f},
    .search_radius = 500.0f,
    .duration_ms = 300000
};
swarm_gateway_send_mission(gw, "ALPHA", &mission);

// Formation
formation_cmd_t formation = {
    .formation_type = 2,  // Diamond
    .center_position = {50.0f, 50.0f, 100.0f},
    .spacing = 10.0f
};
swarm_gateway_send_formation_cmd(gw, "ALPHA", &formation);

// Threat (NULL = broadcast to all)
threat_intel_t threat = {
    .threat_id = 42,
    .threat_level = 8,
    .position = {100.0f, 200.0f, 0.0f}
};
swarm_gateway_send_threat_intel(gw, NULL, &threat);

// Recall
swarm_gateway_send_recall(gw, "ALPHA", true);  // Emergency

// Neuromodulator override
neuromod_override_t override = {
    .modulator_type = DOPAMINE,
    .override_value = 0.8f,
    .duration_ms = 60000
};
swarm_gateway_send_neuromod_override(gw, "ALPHA", &override);
```

### Learning Sync
```c
// Automatic sync (in process loop)
swarm_gateway_sync_learning(gw);

// Manual update creation
learning_update_t update;
swarm_gateway_create_learning_update(gw, &update);
swarm_gateway_send_learning_update(gw, "ALPHA", &update);
swarm_gateway_free_learning_update(&update);
```

### Receive Telemetry
```c
// Poll
swarm_telemetry_t telemetry;
if (swarm_gateway_receive_telemetry(gw, "ALPHA", &telemetry) == 0) {
    printf("Battery: %.1f%%\n", telemetry.avg_battery_level);
}

// Callback
void on_telem(const char* id, const swarm_telemetry_t* t, void* ud) {
    printf("Swarm %s: %u drones\n", id, t->num_drones);
}
swarm_gateway_register_telemetry_callback(gw, on_telem, NULL);
```

### Health & Status
```c
// Health
swarm_health_t health;
swarm_gateway_get_swarm_status(gw, "ALPHA", &health);
printf("Status: %s, Health: %.1f%%\n",
       swarm_gateway_status_to_string(health.status),
       health.overall_health * 100.0f);

// List connected swarms
char swarms[10][32];
int n = swarm_gateway_get_connected_swarms(gw, swarms, 10);
for (int i = 0; i < n; i++) {
    printf("Swarm: %s\n", swarms[i]);
}

// Statistics
uint32_t num_swarms, total_drones;
uint64_t msgs_sent, msgs_received;
swarm_gateway_get_stats(gw, &num_swarms, &total_drones,
                       &msgs_sent, &msgs_received);
```

### Main Loop
```c
while (running) {
    // Process (checks timeouts, sends heartbeats, receives telemetry)
    int events = swarm_gateway_process(gw, 100);

    // Periodic learning sync
    if (should_sync_learning()) {
        swarm_gateway_sync_learning(gw);
    }

    // Aggregate to server
    if (should_aggregate()) {
        swarm_gateway_aggregate_to_server(gw);
    }
}
```

### Message Types
```c
GATEWAY_MSG_LEARNING_UPDATE    // Weight deltas
GATEWAY_MSG_MISSION_PARAMS     // Mission objectives
GATEWAY_MSG_THREAT_INTEL       // Threat information
GATEWAY_MSG_FORMATION_CMD      // Formation commands
GATEWAY_MSG_RECALL             // Return to base
GATEWAY_MSG_NEUROMOD_OVERRIDE  // Behavioral control
GATEWAY_MSG_HEARTBEAT          // Keep-alive
GATEWAY_MSG_SYNC_REQUEST       // Sync request
```

### Status Types
```c
SWARM_STATUS_DISCONNECTED      // Not connected
SWARM_STATUS_CONNECTING        // Connection in progress
SWARM_STATUS_CONNECTED         // Active connection
SWARM_STATUS_DEGRADED          // Partial connectivity
SWARM_STATUS_TIMEOUT           // Connection timeout
```

### Return Codes
```c
0           // Success
-EINVAL     // Invalid arguments
-ENOENT     // Swarm not found
-ENOTCONN   // Not connected
-EPERM      // Operation not permitted
-ENOSPC     // At max capacity
-EEXIST     // Already connected
-EAGAIN     // No data available
```

### Complete Example
```c
#include "swarm/nimcp_swarm_gateway.h"

int main() {
    // Create brain
    brain_t* brain = brain_create(&brain_config);

    // Create gateway
    swarm_gateway_config_t gw_config = { /* ... */ };
    swarm_gateway_t* gw = swarm_gateway_create(brain, &gw_config);

    // Connect swarms
    swarm_gateway_connect_swarm(gw, "ALPHA", "192.168.1.100:8000");
    swarm_gateway_connect_swarm(gw, "BETA", "192.168.1.101:8000");

    // Register callbacks
    swarm_gateway_register_telemetry_callback(gw, on_telemetry, NULL);

    // Send mission
    mission_params_t mission = { /* ... */ };
    swarm_gateway_send_mission(gw, "ALPHA", &mission);

    // Main loop
    while (running) {
        swarm_gateway_process(gw, 100);

        if (should_sync()) {
            swarm_gateway_sync_learning(gw);
        }
    }

    // Cleanup
    swarm_gateway_destroy(gw);
    brain_destroy(brain);

    return 0;
}
```

### Configuration Tips

| Scenario | broadcast_ms | timeout_ms | max_swarms |
|----------|--------------|------------|------------|
| Low-latency | 500 | 2000 | 5 |
| Standard | 1000 | 5000 | 10 |
| Large-scale | 2000 | 10000 | 50 |
| Resource-limited | 5000 | 15000 | 3 |

### Performance
- Max swarms: 1000 (configurable)
- Throughput: 10,000 msg/s
- Latency: 10-100 ms
- Learning compression: 10-1000x

### See Also
- `docs/SWARM_GATEWAY_GUIDE.md` - Full guide
- `examples/swarm_gateway_demo.c` - Complete example
- `docs/SWARM_MODULE_README.md` - Module overview
