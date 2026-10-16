/**
 * @file multi_swarm_demo.c
 * @brief Demonstration of Multi-Swarm Coordination System
 *
 * This example shows how to use the NIMCP Multi-Swarm Coordination
 * system to coordinate multiple swarms for joint missions.
 */

#include "swarm/nimcp_swarm_multi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * @brief Main demonstration function
 */
int main(void) {
    printf("\n=== NIMCP Multi-Swarm Coordination Demo ===\n\n");

    /* Create multi-swarm coordinator */
    printf("Creating multi-swarm coordinator...\n");
    nimcp_multi_swarm_coordinator_t* coordinator =
        nimcp_multi_swarm_create(NULL, NULL);
    if (!coordinator) {
        fprintf(stderr, "Failed to create coordinator\n");
        return 1;
    }

    /* Create super-swarm */
    printf("Creating super-swarm 'Alpha Battalion'...\n");
    nimcp_super_swarm_t* super =
        nimcp_super_swarm_create(coordinator, "Alpha Battalion");
    if (!super) {
        fprintf(stderr, "Failed to create super-swarm\n");
        nimcp_multi_swarm_destroy(coordinator);
        return 1;
    }

    /* Create reconnaissance swarm */
    printf("Creating reconnaissance swarm...\n");
    nimcp_swarm_identity_t* recon =
        nimcp_swarm_identity_create(coordinator, "Recon-1", 20);
    if (!recon) {
        fprintf(stderr, "Failed to create recon swarm\n");
        nimcp_multi_swarm_destroy(coordinator);
        return 1;
    }

    /* Add reconnaissance capabilities */
    nimcp_swarm_add_capability(recon, NIMCP_SWARM_CAP_RECONNAISSANCE,
                               0.9f, 20, true);
    nimcp_swarm_add_capability(recon, NIMCP_SWARM_CAP_SURVEILLANCE,
                               0.85f, 20, true);

    /* Create transport swarm */
    printf("Creating transport swarm...\n");
    nimcp_swarm_identity_t* transport =
        nimcp_swarm_identity_create(coordinator, "Transport-1", 15);
    if (!transport) {
        fprintf(stderr, "Failed to create transport swarm\n");
        nimcp_swarm_identity_destroy(recon);
        nimcp_multi_swarm_destroy(coordinator);
        return 1;
    }

    /* Add transport capabilities */
    nimcp_swarm_add_capability(transport, NIMCP_SWARM_CAP_TRANSPORT,
                               0.8f, 15, true);
    nimcp_swarm_add_capability(transport, NIMCP_SWARM_CAP_LOGISTICS,
                               0.7f, 15, true);

    /* Create combat swarm */
    printf("Creating combat swarm...\n");
    nimcp_swarm_identity_t* combat =
        nimcp_swarm_identity_create(coordinator, "Combat-1", 25);
    if (!combat) {
        fprintf(stderr, "Failed to create combat swarm\n");
        nimcp_swarm_identity_destroy(transport);
        nimcp_swarm_identity_destroy(recon);
        nimcp_multi_swarm_destroy(coordinator);
        return 1;
    }

    /* Add combat capabilities */
    nimcp_swarm_add_capability(combat, NIMCP_SWARM_CAP_COMBAT,
                               0.95f, 25, false);
    nimcp_swarm_add_capability(combat, NIMCP_SWARM_CAP_DEFENSE,
                               0.9f, 25, true);

    /* Register all swarms */
    printf("Registering swarms with coordinator...\n");
    nimcp_swarm_register(coordinator, recon);
    nimcp_swarm_register(coordinator, transport);
    nimcp_swarm_register(coordinator, combat);

    /* Add swarms to super-swarm */
    printf("Adding swarms to super-swarm...\n");
    nimcp_super_swarm_add_swarm(super, recon);
    nimcp_super_swarm_add_swarm(super, transport);
    nimcp_super_swarm_add_swarm(super, combat);

    /* Set territories for each swarm */
    printf("Setting swarm territories...\n");
    nimcp_coord3d_t recon_min = {0, 0, 0};
    nimcp_coord3d_t recon_max = {100, 100, 50};
    nimcp_swarm_set_territory(recon, recon_min, recon_max, true, 0.8f);

    nimcp_coord3d_t transport_min = {50, 50, 0};
    nimcp_coord3d_t transport_max = {150, 150, 30};
    nimcp_swarm_set_territory(transport, transport_min, transport_max, true, 0.6f);

    nimcp_coord3d_t combat_min = {80, 80, 10};
    nimcp_coord3d_t combat_max = {180, 180, 60};
    nimcp_swarm_set_territory(combat, combat_min, combat_max, false, 1.0f);

    /* Create communication bridges */
    printf("Creating communication bridges...\n");
    uint64_t bridge_recon_transport = nimcp_comm_bridge_create(
        coordinator, recon->swarm_id, transport->swarm_id, NULL, 0);
    uint64_t bridge_transport_combat = nimcp_comm_bridge_create(
        coordinator, transport->swarm_id, combat->swarm_id, NULL, 0);

    printf("Bridge IDs: Recon-Transport=%lu, Transport-Combat=%lu\n",
           bridge_recon_transport, bridge_transport_combat);

    /* Detect territory conflicts */
    printf("\nDetecting territory conflicts...\n");
    uint32_t conflict_count = nimcp_conflict_detect(coordinator);
    printf("Detected %u territory conflicts\n", conflict_count);

    /* Auto-resolve conflicts */
    if (conflict_count > 0) {
        printf("Auto-resolving conflicts...\n");
        uint32_t resolved = nimcp_conflict_auto_resolve(coordinator, NULL, NULL);
        printf("Resolved %u conflicts\n", resolved);
    }

    /* Create a joint mission */
    printf("\nCreating joint mission...\n");
    nimcp_territory_bounds_t operation_area = {
        .min = {50, 50, 0},
        .max = {150, 150, 50},
        .is_dynamic = false,
        .priority = 1.0f
    };

    uint64_t mission_id = nimcp_mission_create(
        coordinator,
        "Reconnaissance and supply run to target zone Alpha",
        NIMCP_MISSION_PRIORITY_HIGH,
        operation_area,
        0  /* No deadline */
    );

    printf("Created mission ID: %lu\n", mission_id);

    /* Assign swarms to mission */
    uint64_t assigned_swarms[] = {
        recon->swarm_id,
        transport->swarm_id,
        combat->swarm_id
    };

    nimcp_result_t result = nimcp_mission_assign_swarms(
        coordinator, mission_id, assigned_swarms, 3);

    if (result == NIMCP_OK) {
        printf("Successfully assigned 3 swarms to mission\n");
    }

    /* Simulate mission progress */
    printf("\nSimulating mission execution...\n");
    for (float progress = 0.0f; progress <= 1.0f; progress += 0.25f) {
        nimcp_mission_update_progress(coordinator, mission_id, progress);
        printf("Mission progress: %.0f%%\n", progress * 100.0f);

        /* Simulate some agents going offline */
        if (progress > 0.5f) {
            nimcp_swarm_update_health(recon, 18);  /* Lost 2 agents */
            nimcp_swarm_update_health(transport, 14);  /* Lost 1 agent */
        }

        usleep(500000);  /* 0.5 second delay */
    }

    /* Complete mission */
    nimcp_mission_complete(coordinator, mission_id, true);
    printf("Mission completed successfully!\n");

    /* Demonstrate resource sharing */
    printf("\nDemonstrating resource sharing...\n");
    uint64_t request_id = nimcp_resource_request(
        coordinator,
        transport->swarm_id,  /* Requesting swarm */
        recon->swarm_id,      /* Target swarm */
        NIMCP_RESOURCE_REQ_INFORMATION,
        10,  /* Quantity */
        NIMCP_MISSION_PRIORITY_MEDIUM
    );

    printf("Created resource request ID: %lu\n", request_id);
    printf("Transport swarm requesting reconnaissance data\n");

    /* Approve the request */
    nimcp_resource_approve(coordinator, request_id, 0.1f);
    printf("Resource request approved with cost: 0.1\n");

    /* Query swarms by capability */
    printf("\nQuerying swarms by capability...\n");
    nimcp_vector_t* results = nimcp_vector_create(sizeof(uint64_t));

    uint32_t found = nimcp_swarm_find_by_capability(
        coordinator,
        NIMCP_SWARM_CAP_SURVEILLANCE,
        0.5f,  /* Minimum proficiency */
        results
    );

    printf("Found %u swarms with surveillance capability (>= 0.5 proficiency)\n", found);
    nimcp_vector_destroy(results);

    /* Get overall statistics */
    printf("\n");
    uint32_t total_swarms, total_agents, active_missions, active_conflicts;
    nimcp_multi_swarm_get_stats(coordinator,
                                &total_swarms,
                                &total_agents,
                                &active_missions,
                                &active_conflicts);

    printf("=== Final Statistics ===\n");
    printf("Total Swarms:     %u\n", total_swarms);
    printf("Total Agents:     %u\n", total_agents);
    printf("Active Missions:  %u\n", active_missions);
    printf("Active Conflicts: %u\n", active_conflicts);

    /* Print detailed status */
    printf("\n");
    nimcp_multi_swarm_print_status(coordinator, true);

    /* Cleanup */
    printf("Cleaning up...\n");
    nimcp_multi_swarm_destroy(coordinator);

    printf("\n=== Demo Complete ===\n\n");
    return 0;
}
