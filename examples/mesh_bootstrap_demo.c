/**
 * @file mesh_bootstrap_demo.c
 * @brief Demonstration of NIMCP Mesh Network Bootstrap
 *
 * Shows the complete mesh network in action:
 * 1. Bootstrap creates all channels and registers ~260 modules
 * 2. Modules are assigned to channels by category
 * 3. Endorsement policies control transaction validation
 * 4. Gossip propagates beliefs across participants
 * 5. FEP-based convergence minimizes free energy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_transaction.h"

/* ============================================================================
 * Demo: Show Bootstrap Status
 * ============================================================================ */

static void demo_bootstrap_status(mesh_bootstrap_t* bootstrap) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                    MESH BOOTSTRAP DEMO                            ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    /* Print the built-in status */
    mesh_bootstrap_print_status(bootstrap);
}

/* ============================================================================
 * Demo: Channel Operations
 * ============================================================================ */

static void demo_channel_operations(mesh_bootstrap_t* bootstrap) {
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│                    CHANNEL OPERATIONS                             │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");

    /* Get channels */
    mesh_channel_t* system_ch = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SYSTEM);
    mesh_channel_t* left_ch = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_LEFT_HEMISPHERE);
    mesh_channel_t* right_ch = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_RIGHT_HEMISPHERE);
    mesh_channel_t* subcortical_ch = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SUBCORTICAL);
    mesh_channel_t* gpu_ch = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_GPU_COMPUTE);

    printf("Channels created:\n");
    printf("  [0] SYSTEM         - Infrastructure, security, routing\n");
    printf("  [1] LEFT_HEMISPHERE  - Cognitive, reasoning, language\n");
    printf("  [2] RIGHT_HEMISPHERE - Perception, spatial, creative\n");
    printf("  [3] SUBCORTICAL    - Limbic, motor, memory\n");
    printf("  [4] GPU_COMPUTE    - Parallel processing, batch ops\n");
    printf("\n");

    /* Show channel stats */
    printf("Channel Statistics:\n");
    printf("  %-20s  Participants  World State Items  Free Energy\n", "Channel");
    printf("  %-20s  ────────────  ─────────────────  ───────────\n", "────────────────────");

    mesh_channel_t* channels[] = {system_ch, left_ch, right_ch, subcortical_ch, gpu_ch};
    const char* names[] = {"SYSTEM", "LEFT_HEMISPHERE", "RIGHT_HEMISPHERE", "SUBCORTICAL", "GPU_COMPUTE"};

    for (int i = 0; i < 5; i++) {
        if (channels[i]) {
            mesh_channel_stats_t stats;
            mesh_channel_get_stats(channels[i], &stats);
            printf("  %-20s  %12zu  %17zu  %11.4f\n",
                   names[i],
                   stats.participant_count,
                   stats.world_state_items,
                   mesh_channel_get_free_energy(channels[i]));
        }
    }
    printf("\n");
}

/* ============================================================================
 * Demo: Belief Introduction and Gossip
 * ============================================================================ */

static void demo_belief_gossip(mesh_bootstrap_t* bootstrap) {
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│                    BELIEF PROPAGATION                             │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");

    mesh_channel_t* left_ch = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_LEFT_HEMISPHERE);
    if (!left_ch) {
        printf("ERROR: Could not get left hemisphere channel\n");
        return;
    }

    /* Create a belief (e.g., "2+2=4" fact encoded as neural vector) */
    printf("Introducing belief: ID=42, certainty=0.95, vector encoding of '4'\n");
    printf("\n");

    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 42;
    belief.source = 0x1001;  /* From a cognitive module */
    belief.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    belief.certainty = 0.95f;
    belief.vector_dim = 4;
    /* Encode "4" as a simple pattern in belief vector */
    belief.belief_vector[0] = 0.0f;
    belief.belief_vector[1] = 1.0f;
    belief.belief_vector[2] = 0.0f;
    belief.belief_vector[3] = 0.0f;
    belief.timestamp_ns = 1000000;
    belief.propagation_count = 0;

    mesh_channel_introduce_belief(left_ch, &belief);

    /* Show state before gossip */
    mesh_channel_stats_t before;
    mesh_channel_get_stats(left_ch, &before);
    printf("Before gossip:\n");
    printf("  World state items: %zu\n", before.world_state_items);
    printf("  Free energy: %.4f\n", mesh_channel_get_free_energy(left_ch));
    printf("\n");

    /* Run gossip rounds */
    printf("Running 3 gossip rounds...\n");
    for (int i = 0; i < 3; i++) {
        mesh_channel_gossip_round(left_ch);
        printf("  Round %d: Free energy = %.4f\n",
               i + 1, mesh_channel_get_free_energy(left_ch));
    }
    printf("\n");

    /* Show state after gossip */
    mesh_channel_stats_t after;
    mesh_channel_get_stats(left_ch, &after);
    printf("After gossip:\n");
    printf("  World state items: %zu\n", after.world_state_items);
    printf("  Free energy: %.4f\n", mesh_channel_get_free_energy(left_ch));
    printf("  Converged: %s\n", mesh_channel_has_converged(left_ch) ? "YES" : "NO");
    printf("\n");
}

/* ============================================================================
 * Demo: Cross-Channel Communication
 * ============================================================================ */

static void demo_cross_channel(mesh_bootstrap_t* bootstrap) {
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│                  CROSS-CHANNEL ROUTING                            │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");

    printf("Cross-channel routing flow (via Thalamus gateway):\n");
    printf("\n");
    printf("  LEFT_HEMISPHERE ──┐\n");
    printf("                    │\n");
    printf("  RIGHT_HEMISPHERE ─┼──► THALAMUS (GATEWAY) ──► TARGET CHANNEL\n");
    printf("                    │        │\n");
    printf("  SUBCORTICAL ──────┘        ▼\n");
    printf("                      Cross-channel TX\n");
    printf("                      requires policy:\n");
    printf("                      'left_leader AND right_leader'\n");
    printf("\n");

    printf("Example cross-channel transaction types:\n");
    printf("  - MESH_TX_CROSS_CHANNEL: Routed between hemispheres\n");
    printf("  - MESH_TX_EMERGENCY_OVERRIDE: Amygdala VETO can block\n");
    printf("  - MESH_TX_GPU_BATCH: Offloaded to GPU channel\n");
    printf("\n");
}

/* ============================================================================
 * Demo: Endorsement Policies
 * ============================================================================ */

static void demo_endorsement_policies(void) {
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│                  ENDORSEMENT POLICIES                             │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");

    printf("Policies control which modules must endorse transactions:\n");
    printf("\n");
    printf("  Policy                Expression                      Endorsers\n");
    printf("  ──────────────────    ────────────────────────────    ─────────────────\n");
    printf("  cognitive             PFC                             PFC (REQUIRED)\n");
    printf("  motor_command         motor_cortex AND cerebellum     Both REQUIRED\n");
    printf("  memory_store          hippocampus AND (PFC OR sal>0.7) Hippocampus REQ\n");
    printf("  sensory_fusion        ANY 2 OF (visual,audio,soma)    Any 2 of 3\n");
    printf("  emergency             amygdala                        Amygdala (VETO)\n");
    printf("  cross_hemisphere      left_leader AND right_leader    Both PFCs\n");
    printf("  security              bbb AND immune                  BBB+Immune REQ\n");
    printf("  gpu_batch             gpu_coordinator                 GPU coord REQ\n");
    printf("\n");

    printf("Endorser roles:\n");
    printf("  REQUIRED  - Must endorse for policy to be satisfied\n");
    printf("  OPTIONAL  - Can endorse but not required\n");
    printf("  VETO      - Can reject and block the transaction\n");
    printf("  ANY_N     - Part of 'any N of M' group\n");
    printf("\n");
}

/* ============================================================================
 * Demo: Transaction Flow
 * ============================================================================ */

static void demo_transaction_flow(void) {
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│                  TRANSACTION FLOW (EOV)                           │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");

    printf("Execute-Order-Validate (Hyperledger Fabric inspired):\n");
    printf("\n");
    printf("  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │  1. EXECUTE                                                 │\n");
    printf("  │     Client proposes transaction                             │\n");
    printf("  │     Endorsers simulate and sign                             │\n");
    printf("  │     Collect endorsements per policy                         │\n");
    printf("  └─────────────────────────────────────────────────────────────┘\n");
    printf("                              │\n");
    printf("                              ▼\n");
    printf("  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │  2. ORDER                                                   │\n");
    printf("  │     Submit to Ordering Service (Raft-based)                 │\n");
    printf("  │     Batch transactions into blocks                          │\n");
    printf("  │     Sequence blocks with consensus                          │\n");
    printf("  └─────────────────────────────────────────────────────────────┘\n");
    printf("                              │\n");
    printf("                              ▼\n");
    printf("  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │  3. VALIDATE                                                │\n");
    printf("  │     Verify endorsement signatures                           │\n");
    printf("  │     Check read-write set conflicts                          │\n");
    printf("  │     Apply valid transactions to world state                 │\n");
    printf("  └─────────────────────────────────────────────────────────────┘\n");
    printf("\n");
}

/* ============================================================================
 * Demo: System Update Cycle
 * ============================================================================ */

static void demo_update_cycle(mesh_bootstrap_t* bootstrap) {
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│                    UPDATE CYCLE                                   │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");

    printf("Running 10 update cycles (10ms each)...\n");
    printf("\n");

    float initial_fe = mesh_bootstrap_get_free_energy(bootstrap);
    printf("  Initial system free energy: %.4f\n", initial_fe);

    for (int i = 0; i < 10; i++) {
        mesh_bootstrap_update(bootstrap, 10);
        mesh_bootstrap_gossip_all(bootstrap, 1);

        if (i == 4 || i == 9) {
            printf("  After cycle %2d: Free energy = %.4f, Converged = %s\n",
                   i + 1,
                   mesh_bootstrap_get_free_energy(bootstrap),
                   mesh_bootstrap_has_converged(bootstrap) ? "YES" : "NO");
        }
    }

    size_t processed = mesh_bootstrap_process_transactions(bootstrap);
    printf("\n");
    printf("  Transactions processed: %zu\n", processed);
    printf("  Final system free energy: %.4f\n", mesh_bootstrap_get_free_energy(bootstrap));
    printf("\n");
}

/* ============================================================================
 * Demo: Module Categories
 * ============================================================================ */

static void demo_module_categories(void) {
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│                  MODULE CATEGORIES                                │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");

    printf("Category             Default Channel       Example Modules\n");
    printf("───────────────────  ──────────────────    ─────────────────────────\n");
    printf("COGNITIVE            LEFT_HEMISPHERE       fep_orchestrator, reasoning\n");
    printf("PERCEPTION           RIGHT_HEMISPHERE      visual_cortex, auditory\n");
    printf("SUBCORTICAL          SUBCORTICAL           amygdala, hippocampus\n");
    printf("MOTOR                SUBCORTICAL           motor_cortex, cerebellum\n");
    printf("MEMORY               SUBCORTICAL           episodic_memory, working_mem\n");
    printf("SECURITY             SYSTEM                bbb, immune_system\n");
    printf("GPU                  GPU_COMPUTE           gpu_recovery, batch_proc\n");
    printf("PLASTICITY           SYSTEM                stdp_module, ltp_module\n");
    printf("GLIAL                SYSTEM                astrocytes, microglia\n");
    printf("SWARM                SYSTEM                gossip_beliefs, consensus\n");
    printf("SYSTEM               SYSTEM                bio_router, thalamus\n");
    printf("\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char* argv[]) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("          NIMCP MESH NETWORK BOOTSTRAP DEMONSTRATION                \n");
    printf("═══════════════════════════════════════════════════════════════════\n");

    /* Create bootstrap with all subsystems */
    printf("\nCreating mesh bootstrap with all subsystems...\n");

    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.verbose_logging = false;  /* Quiet mode */

    mesh_bootstrap_t* bootstrap = mesh_bootstrap_create(&config);
    if (!bootstrap) {
        printf("ERROR: Failed to create mesh bootstrap\n");
        return 1;
    }

    /* Run demos */
    demo_bootstrap_status(bootstrap);
    demo_module_categories();
    demo_channel_operations(bootstrap);
    demo_endorsement_policies();
    demo_transaction_flow();
    demo_belief_gossip(bootstrap);
    demo_cross_channel(bootstrap);
    demo_update_cycle(bootstrap);

    /* Final status */
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("                      DEMONSTRATION COMPLETE                        \n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("\n");

    /* Cleanup */
    mesh_bootstrap_destroy(bootstrap);

    return 0;
}
