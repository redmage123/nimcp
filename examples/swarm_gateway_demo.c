/**
 * @file swarm_gateway_demo.c
 * @brief Demonstration of NIMCP Server-to-Swarm Gateway
 *
 * This example shows how to:
 * 1. Create a gateway with a server brain
 * 2. Connect to multiple drone swarms
 * 3. Send mission parameters and learning updates
 * 4. Receive and process telemetry
 * 5. Monitor swarm health
 * 6. Handle swarm events
 */

#include "swarm/nimcp_swarm_gateway.h"
#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* ========================================================================
 * Callback Handlers
 * ======================================================================== */

/**
 * @brief Telemetry callback - called when telemetry arrives from swarms
 */
static void on_telemetry_received(const char* swarm_id,
                                  const swarm_telemetry_t* telemetry,
                                  void* user_data) {
    printf("\n=== TELEMETRY FROM SWARM '%s' ===\n", swarm_id);
    printf("  Active Drones: %u / %u\n",
           telemetry->num_responsive, telemetry->num_drones);
    printf("  Battery Level: %.1f%%\n", telemetry->avg_battery_level);
    printf("  CPU Usage: %.1f%%\n", telemetry->avg_cpu_usage);
    printf("  Memory Usage: %.1f%%\n", telemetry->avg_memory_usage);
    printf("  Formation Coherence: %.2f\n", telemetry->formation_coherence);
    printf("  Mission Progress: %.1f%%\n", telemetry->mission_progress * 100.0f);
    printf("  Communication Health: %u/100\n", telemetry->communication_health);
    printf("  Threats Detected: %u\n", telemetry->num_threats_detected);
    printf("  Center of Mass: (%.2f, %.2f, %.2f)\n",
           telemetry->center_of_mass[0],
           telemetry->center_of_mass[1],
           telemetry->center_of_mass[2]);
}

/**
 * @brief Swarm event callback - called for significant swarm events
 */
static void on_swarm_event(const char* swarm_id,
                          const char* event_type,
                          const void* event_data,
                          void* user_data) {
    printf("\n!!! SWARM EVENT: '%s' - %s !!!\n", swarm_id, event_type);

    if (strcmp(event_type, "timeout") == 0) {
        uint32_t timeout_ms = *(uint32_t*)event_data;
        printf("  No contact for %u ms\n", timeout_ms);
    }
}

/* ========================================================================
 * Main Demonstration
 * ======================================================================== */

int main(int argc, char** argv) {
    printf("=================================================\n");
    printf("  NIMCP Server-to-Swarm Gateway Demonstration\n");
    printf("=================================================\n\n");

    /* Initialize logging */
    nimcp_log_init();
    nimcp_log_set_level(NIMCP_LOG_LEVEL_INFO);

    /* ====================================================================
     * STEP 1: Create Server Brain
     * ==================================================================== */
    printf("Step 1: Creating server brain...\n");

    brain_config_t brain_config = {
        .num_neurons = 10000,
        .num_regions = 10,
        .enable_plasticity = true,
        .enable_oscillations = true,
        .learning_rate = 0.01f
    };

    brain_t* server_brain = brain_create(&brain_config);
    if (!server_brain) {
        fprintf(stderr, "Failed to create server brain\n");
        return 1;
    }

    printf("  ✓ Server brain created with %u neurons\n\n",
           brain_config.num_neurons);

    /* ====================================================================
     * STEP 2: Create Gateway
     * ==================================================================== */
    printf("Step 2: Creating swarm gateway...\n");

    swarm_gateway_config_t gateway_config = {
        .gateway_name = "MISSION_CONTROL",
        .max_swarms = 10,
        .broadcast_interval_ms = 1000,
        .timeout_ms = 5000,
        .enable_learning_sync = true,
        .enable_mission_control = true,
        .enable_telemetry = true
    };

    swarm_gateway_t* gateway = swarm_gateway_create(server_brain, &gateway_config);
    if (!gateway) {
        fprintf(stderr, "Failed to create gateway\n");
        brain_destroy(server_brain);
        return 1;
    }

    printf("  ✓ Gateway '%s' created\n", gateway_config.gateway_name);
    printf("  ✓ Max swarms: %u\n", gateway_config.max_swarms);
    printf("  ✓ Broadcast interval: %u ms\n", gateway_config.broadcast_interval_ms);
    printf("  ✓ Features: Learning=%s, Mission=%s, Telemetry=%s\n\n",
           gateway_config.enable_learning_sync ? "ON" : "OFF",
           gateway_config.enable_mission_control ? "ON" : "OFF",
           gateway_config.enable_telemetry ? "ON" : "OFF");

    /* ====================================================================
     * STEP 3: Register Callbacks
     * ==================================================================== */
    printf("Step 3: Registering callbacks...\n");

    swarm_gateway_register_telemetry_callback(gateway, on_telemetry_received, NULL);
    swarm_gateway_register_event_callback(gateway, on_swarm_event, NULL);

    printf("  ✓ Telemetry callback registered\n");
    printf("  ✓ Event callback registered\n\n");

    /* ====================================================================
     * STEP 4: Connect to Swarms
     * ==================================================================== */
    printf("Step 4: Connecting to swarms...\n");

    const char* swarms[][2] = {
        {"ALPHA_SQUAD", "192.168.1.100:8000"},
        {"BETA_SQUAD", "192.168.1.101:8000"},
        {"GAMMA_SQUAD", "192.168.1.102:8000"}
    };

    for (int i = 0; i < 3; i++) {
        if (swarm_gateway_connect_swarm(gateway, swarms[i][0], swarms[i][1]) == 0) {
            printf("  ✓ Connected to swarm '%s' at %s\n",
                   swarms[i][0], swarms[i][1]);
        } else {
            printf("  ✗ Failed to connect to swarm '%s'\n", swarms[i][0]);
        }
    }

    printf("\n");

    /* ====================================================================
     * STEP 5: Send Mission to ALPHA_SQUAD
     * ==================================================================== */
    printf("Step 5: Sending mission to ALPHA_SQUAD...\n");

    mission_params_t mission = {
        .mission_id = "RECON_001",
        .mission_type = 1,  // Reconnaissance
        .target_coordinates = {100.0f, 200.0f, 50.0f},
        .search_radius = 500.0f,
        .duration_ms = 300000,  // 5 minutes
        .num_objectives = 3,
        .objective_data = NULL,
        .objective_size = 0
    };

    if (swarm_gateway_send_mission(gateway, "ALPHA_SQUAD", &mission) == 0) {
        printf("  ✓ Mission '%s' sent to ALPHA_SQUAD\n", mission.mission_id);
        printf("    Target: (%.1f, %.1f, %.1f)\n",
               mission.target_coordinates[0],
               mission.target_coordinates[1],
               mission.target_coordinates[2]);
        printf("    Duration: %u ms\n", mission.duration_ms);
    }

    printf("\n");

    /* ====================================================================
     * STEP 6: Send Formation Command to BETA_SQUAD
     * ==================================================================== */
    printf("Step 6: Sending formation command to BETA_SQUAD...\n");

    formation_cmd_t formation = {
        .formation_type = 2,  // Diamond formation
        .center_position = {50.0f, 50.0f, 100.0f},
        .spacing = 10.0f,
        .orientation = 45.0f,
        .transition_time_ms = 5000
    };

    if (swarm_gateway_send_formation_cmd(gateway, "BETA_SQUAD", &formation) == 0) {
        printf("  ✓ Formation command sent to BETA_SQUAD\n");
        printf("    Type: Diamond (ID=%u)\n", formation.formation_type);
        printf("    Center: (%.1f, %.1f, %.1f)\n",
               formation.center_position[0],
               formation.center_position[1],
               formation.center_position[2]);
        printf("    Spacing: %.1f m\n", formation.spacing);
    }

    printf("\n");

    /* ====================================================================
     * STEP 7: Broadcast Threat Intelligence
     * ==================================================================== */
    printf("Step 7: Broadcasting threat intelligence...\n");

    threat_intel_t threat = {
        .threat_id = 42,
        .threat_level = 7,
        .position = {120.0f, 180.0f, 0.0f},
        .velocity = {-5.0f, -3.0f, 0.0f},
        .detection_time = 1234567890,
        .threat_type = "HOSTILE_AIRCRAFT"
    };

    int swarms_notified = swarm_gateway_send_threat_intel(gateway, NULL, &threat);
    if (swarms_notified > 0) {
        printf("  ✓ Threat #%u broadcast to %d swarms\n",
               threat.threat_id, swarms_notified);
        printf("    Level: %u/10\n", threat.threat_level);
        printf("    Position: (%.1f, %.1f, %.1f)\n",
               threat.position[0], threat.position[1], threat.position[2]);
        printf("    Type: %s\n", threat.threat_type);
    }

    printf("\n");

    /* ====================================================================
     * STEP 8: Synchronize Learning Updates
     * ==================================================================== */
    printf("Step 8: Synchronizing learning updates...\n");

    int synced = swarm_gateway_sync_learning(gateway);
    if (synced > 0) {
        printf("  ✓ Learning synchronized to %d swarms\n", synced);
        printf("    Server brain weights propagated to drones\n");
    }

    printf("\n");

    /* ====================================================================
     * STEP 9: Process Gateway Events (Main Loop)
     * ==================================================================== */
    printf("Step 9: Running gateway processing loop...\n");
    printf("  (Processing for 10 seconds, press Ctrl+C to exit)\n\n");

    for (int i = 0; i < 10; i++) {
        /* Process gateway - handles timeouts, heartbeats, telemetry */
        int events = swarm_gateway_process(gateway, 100);

        /* Get and display swarm status */
        if (i % 3 == 0) {
            printf("\n--- Swarm Status Update (t=%d s) ---\n", i);

            char swarm_ids[10][32];
            int num_swarms = swarm_gateway_get_connected_swarms(
                gateway, swarm_ids, 10);

            for (int j = 0; j < num_swarms; j++) {
                swarm_health_t health;
                if (swarm_gateway_get_swarm_status(gateway, swarm_ids[j], &health) == 0) {
                    printf("  Swarm '%s': %s (health=%.1f%%, drones=%u/%u, latency=%.1fms)\n",
                           health.swarm_id,
                           swarm_gateway_status_to_string(health.status),
                           health.overall_health * 100.0f,
                           health.num_drones_active,
                           health.num_drones_total,
                           health.latency_ms);
                }
            }
        }

        sleep(1);
    }

    printf("\n");

    /* ====================================================================
     * STEP 10: Aggregate Data to Server
     * ==================================================================== */
    printf("Step 10: Aggregating swarm data to server...\n");

    int aggregated = swarm_gateway_aggregate_to_server(gateway);
    printf("  ✓ Aggregated data from %d swarms\n", aggregated);
    printf("    Server brain updated with macro-level insights\n\n");

    /* ====================================================================
     * STEP 11: Get Gateway Statistics
     * ==================================================================== */
    printf("Step 11: Gateway statistics:\n");

    uint32_t num_swarms, total_drones;
    uint64_t msgs_sent, msgs_received;

    swarm_gateway_get_stats(gateway, &num_swarms, &total_drones,
                           &msgs_sent, &msgs_received);

    printf("  Connected Swarms: %u\n", num_swarms);
    printf("  Total Drones: %u\n", total_drones);
    printf("  Messages Sent: %lu\n", msgs_sent);
    printf("  Messages Received: %lu\n", msgs_received);

    printf("\n");

    /* ====================================================================
     * STEP 12: Send Recall Command (Emergency Return)
     * ==================================================================== */
    printf("Step 12: Sending emergency recall to all swarms...\n");

    char recall_swarms[10][32];
    int num_recall = swarm_gateway_get_connected_swarms(gateway, recall_swarms, 10);

    for (int i = 0; i < num_recall; i++) {
        if (swarm_gateway_send_recall(gateway, recall_swarms[i], true) == 0) {
            printf("  ✓ Emergency recall sent to '%s'\n", recall_swarms[i]);
        }
    }

    printf("\n");

    /* ====================================================================
     * STEP 13: Cleanup
     * ==================================================================== */
    printf("Step 13: Cleaning up...\n");

    swarm_gateway_destroy(gateway);
    printf("  ✓ Gateway destroyed\n");

    brain_destroy(server_brain);
    printf("  ✓ Server brain destroyed\n");

    printf("\n=================================================\n");
    printf("  Demonstration Complete!\n");
    printf("=================================================\n");

    return 0;
}
