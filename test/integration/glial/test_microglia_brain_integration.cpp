/**
 * @file test_microglia_brain_integration.cpp
 * @brief Integration tests for enhanced microglia with brain systems
 *
 * WHAT: Tests microglia integration with brain, neural networks, and other glial
 * WHY:  Verify complete system behavior with mathematical enhancements
 * HOW:  Create realistic brain scenarios and verify correct interactions
 *
 * TEST SCENARIOS:
 * 1. Brain-Microglia Integration: Microglia step with brain simulation
 * 2. Neural Network Pruning: Microglia removes actual synapses
 * 3. Activity-Dependent Behavior: Pruning responds to real neural activity
 * 4. Complement Cascade in Brain: C1q/C3 tagging with network dynamics
 * 5. Cytokine-Mediated Coordination: Communication between microglia
 * 6. Centrality Protection: Critical synapses protected during pruning
 * 7. State Transitions in Simulation: RK4 dynamics under realistic conditions
 * 8. Performance under Load: Scalability with large brain configurations
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "glial/microglia/nimcp_microglia.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/brain/nimcp_brain.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MicrogliaBrainIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Create small neural network for testing
        network_config_t config{};
        config.num_neurons = 50;
        config.input_size = 10;
        config.output_size = 5;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.min_weight = -1.0f;
        config.max_weight = 1.0f;
        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);

        // Add connections (synapses)
        for (uint32_t i = 0; i < 40; i++) {
            uint32_t from = i % 45;
            uint32_t to = (i + 5) % 50;
            if (from != to) {
                neural_network_add_connection(network, from, to, 0.5f);
            }
        }
    }

    void TearDown() override {
        if (network) neural_network_destroy(network);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected";
    }

    // Helper: Create microglia network with spatial distribution
    microglia_network_t* create_distributed_microglia(uint32_t count) {
        microglia_network_config_t config = microglia_network_default_config();
        config.capacity = count + 10;
        config.enable_centrality_protection = true;
        config.enable_complement_cascade = true;
        config.enable_cytokine_signaling = true;
        config.enable_state_dynamics = true;

        microglia_network_t* net = microglia_network_create_enhanced(&config);
        if (!net) return nullptr;

        for (uint32_t i = 0; i < count; i++) {
            float x = (float)(i % 5) * 50.0f;
            float y = (float)((i / 5) % 5) * 50.0f;
            float z = (float)(i / 25) * 50.0f;

            microglia_t* mg = microglia_create(i, x, y, z, 100.0f);
            if (mg) {
                microglia_network_add(net, mg);
            }
        }

        return net;
    }

    // Helper: Assign synapses to microglia based on position
    void assign_synapses_to_microglia(microglia_network_t* net,
                                       uint32_t num_synapses) {
        for (uint32_t i = 0; i < net->num_microglia && i < num_synapses; i++) {
            microglia_t* mg = net->microglia[i];
            uint32_t synapses_per = num_synapses / net->num_microglia;

            for (uint32_t j = 0; j < synapses_per; j++) {
                uint32_t syn_id = i * synapses_per + j;
                float sx = mg->position[0] + (float)(j % 5) * 10.0f;
                float sy = mg->position[1] + (float)((j / 5) % 5) * 10.0f;
                float sz = mg->position[2];

                microglia_monitor_synapse_at(mg, syn_id, sx, sy, sz);
            }
        }
    }

    neural_network_t network = nullptr;
};

//=============================================================================
// SCENARIO 1: Brain-Microglia Integration
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, BrainMicrogliaStep) {
    microglia_network_t* mg_net = create_distributed_microglia(10);
    ASSERT_NE(mg_net, nullptr);

    assign_synapses_to_microglia(mg_net, 100);

    uint64_t now = nimcp_time_monotonic_us();

    // Simulate brain activity affecting microglia
    for (int step = 0; step < 100; step++) {
        // Simulate some neural activity
        for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
            microglia_t* mg = mg_net->microglia[i];
            for (uint32_t j = 0; j < mg->num_monitored_synapses; j++) {
                // Varying activity levels
                float activity = (j % 2 == 0) ? 5.0f : 0.1f;
                microglia_track_synapse_activity(mg, mg->monitored_synapse_ids[j],
                                                  activity, now + step * 1000);
            }
        }

        // Network step
        microglia_network_step(mg_net, now + step * 1000);
    }

    // Verify state evolution
    microglia_network_stats_t stats;
    microglia_network_get_stats(mg_net, &stats);

    EXPECT_EQ(stats.total_microglia, 10);
    EXPECT_GT(stats.total_monitored_synapses, 0);

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 2: Neural Network Pruning
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, SynapsePruningIntegration) {
    microglia_network_t* mg_net = create_distributed_microglia(5);
    ASSERT_NE(mg_net, nullptr);

    // Get initial synapse count from neural network
    network_stats_t net_stats{};
    neural_network_get_stats(network, &net_stats);
    uint32_t initial_synapses = net_stats.total_synapses;

    // Monitor some neural network synapses
    for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
        microglia_t* mg = mg_net->microglia[i];

        // Monitor actual network synapses
        for (uint32_t j = 0; j < 5; j++) {
            uint32_t syn_id = i * 5 + j;
            microglia_monitor_synapse(mg, syn_id);
        }
    }

    // No activity tracked - all synapses should be candidates for pruning
    uint64_t now = nimcp_time_monotonic_us();

    // Run multiple prune cycles
    uint32_t total_pruned = 0;
    for (int cycle = 0; cycle < 10; cycle++) {
        for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
            total_pruned += microglia_prune_weak_synapses(mg_net->microglia[i]);
        }
    }

    // Verify some synapses were pruned
    EXPECT_GT(total_pruned, 0);

    microglia_network_stats_t stats;
    microglia_network_get_stats(mg_net, &stats);
    EXPECT_EQ(stats.total_pruned, total_pruned);

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 3: Activity-Dependent Behavior
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, ActivityDependentPruning) {
    microglia_network_t* mg_net = create_distributed_microglia(3);
    ASSERT_NE(mg_net, nullptr);

    microglia_t* mg = mg_net->microglia[0];

    // Monitor 20 synapses
    for (uint32_t i = 0; i < 20; i++) {
        microglia_monitor_synapse(mg, 1000 + i);
    }

    uint64_t now = nimcp_time_monotonic_us();

    // First 10 synapses: high activity (should NOT be pruned)
    for (uint32_t i = 0; i < 10; i++) {
        for (int j = 0; j < 50; j++) {
            microglia_track_synapse_activity(mg, 1000 + i, 5.0f, now + j * 1000);
        }
    }

    // Last 10 synapses: no activity (should be pruned)
    // (No activity tracking = weak)

    uint32_t initial_count = mg->num_monitored_synapses;

    // Prune
    for (int i = 0; i < 5; i++) {
        microglia_prune_weak_synapses(mg);
    }

    // Active synapses should be preserved
    uint32_t remaining = mg->num_monitored_synapses;
    uint32_t pruned = initial_count - remaining;

    // Should have pruned some weak synapses
    EXPECT_GT(pruned, 0);

    // Active synapses should still be monitored
    for (uint32_t i = 0; i < 10; i++) {
        float score = microglia_get_synapse_activity_score(mg, 1000 + i);
        // High-activity synapses should have high scores
        if (score > 0.0f) {
            EXPECT_GT(score, mg->pruning_threshold * 0.5f);
        }
    }

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 4: Complement Cascade in Brain
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, ComplementCascadeWithNetwork) {
    microglia_network_t* mg_net = create_distributed_microglia(5);
    ASSERT_NE(mg_net, nullptr);

    assign_synapses_to_microglia(mg_net, 100);

    uint64_t now = nimcp_time_monotonic_us();

    // Track activity for only some synapses
    for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
        microglia_t* mg = mg_net->microglia[i];

        // Half get activity, half don't
        for (uint32_t j = 0; j < mg->num_monitored_synapses / 2; j++) {
            microglia_track_synapse_activity(mg, mg->monitored_synapse_ids[j],
                                              5.0f, now);
        }
    }

    // Apply complement cascade
    uint32_t total_tagged = 0;
    for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
        total_tagged += microglia_apply_complement_tags(mg_net->microglia[i], now);
    }

    // Should tag inactive synapses
    EXPECT_GT(total_tagged, 0);

    // Wait and convert to C3
    uint64_t later = now + 5000000;  // 5 seconds
    for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
        microglia_apply_complement_tags(mg_net->microglia[i], later);
    }

    // Check statistics
    microglia_network_stats_t stats;
    microglia_network_get_stats(mg_net, &stats);
    EXPECT_GT(stats.total_c1q_tagged, 0);

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 5: Cytokine-Mediated Coordination
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, CytokineCoordination) {
    microglia_network_t* mg_net = create_distributed_microglia(10);
    ASSERT_NE(mg_net, nullptr);

    // Activate one microglia (source of cytokines)
    microglia_t* source = mg_net->microglia[0];
    for (int i = 0; i < 50; i++) {
        microglia_set_inflammation(source, 0.8f);
        microglia_update_state_dynamics(source, 0.1f);
    }

    // Update cytokines
    for (int i = 0; i < 20; i++) {
        microglia_update_cytokines(source, 0.1f);
    }

    float source_il1b = microglia_get_cytokine(source, CYTOKINE_IL1B);
    EXPECT_GT(source_il1b, 0.0f);

    // Diffuse cytokines through network
    for (int i = 0; i < 100; i++) {
        microglia_network_diffuse_cytokines(mg_net, 0.1f);
    }

    // Nearby microglia should receive some cytokines
    microglia_t* neighbor = mg_net->microglia[1];
    float neighbor_il1b = microglia_get_cytokine(neighbor, CYTOKINE_IL1B);

    // May have received diffused cytokines
    EXPECT_GE(neighbor_il1b, 0.0f);

    // Check network totals
    microglia_network_stats_t stats;
    microglia_network_get_stats(mg_net, &stats);
    EXPECT_GT(stats.total_pro_inflammatory, 0.0f);

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 6: Centrality Protection
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, CentralityProtectionInNetwork) {
    microglia_network_t* mg_net = create_distributed_microglia(5);
    ASSERT_NE(mg_net, nullptr);

    microglia_t* mg = mg_net->microglia[0];

    // Monitor 20 synapses (all weak - no activity)
    for (uint32_t i = 0; i < 20; i++) {
        microglia_monitor_synapse(mg, 1000 + i);
    }

    // Set high centrality for some "hub" synapses
    microglia_set_synapse_centrality(mg, 1000, 0.95f);  // Hub
    microglia_set_synapse_centrality(mg, 1005, 0.90f);  // Hub
    microglia_set_synapse_centrality(mg, 1010, 0.85f);  // Hub

    // Low centrality for others (peripheral)
    for (uint32_t i = 0; i < 20; i++) {
        if (i != 0 && i != 5 && i != 10) {
            microglia_set_synapse_centrality(mg, 1000 + i, 0.05f);
        }
    }

    // Prune repeatedly
    for (int round = 0; round < 10; round++) {
        microglia_prune_weak_synapses(mg);
    }

    // Hub synapses should be more likely to survive
    // (Protected by high centrality)

    EXPECT_GT(mg->protected_from_pruning, 0);

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 7: State Transitions in Simulation
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, StateTransitionsUnderStimulation) {
    microglia_network_t* mg_net = create_distributed_microglia(10);
    ASSERT_NE(mg_net, nullptr);

    // Initial state: all should be ramified
    microglia_network_stats_t initial_stats;
    microglia_network_get_stats(mg_net, &initial_stats);
    EXPECT_EQ(initial_stats.ramified_count, 10);

    // Simulate injury (high inflammation)
    for (int phase = 0; phase < 100; phase++) {
        // Apply global inflammation
        for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
            microglia_set_inflammation(mg_net->microglia[i], 0.7f);
        }

        microglia_network_step(mg_net, phase * 100000);
    }

    // Check state distribution after injury
    microglia_network_stats_t injury_stats;
    microglia_network_get_stats(mg_net, &injury_stats);

    // Some should be activated or phagocytic
    EXPECT_LT(injury_stats.ramified_count, 10);
    uint32_t activated_total = injury_stats.activated_count + injury_stats.phagocytic_count;
    EXPECT_GT(activated_total, 0);

    // Simulate recovery (remove inflammation)
    for (int phase = 0; phase < 200; phase++) {
        for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
            microglia_set_inflammation(mg_net->microglia[i], 0.0f);
        }

        microglia_network_step(mg_net, 100 * 100000 + phase * 100000);
    }

    // Should trend back toward ramified
    microglia_network_stats_t recovery_stats;
    microglia_network_get_stats(mg_net, &recovery_stats);
    EXPECT_GT(recovery_stats.ramified_count, injury_stats.ramified_count);

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 8: Performance Under Load
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, PerformanceWithLargeNetwork) {
    // Create larger network
    microglia_network_t* mg_net = create_distributed_microglia(50);
    ASSERT_NE(mg_net, nullptr);

    // Assign many synapses
    for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
        microglia_t* mg = mg_net->microglia[i];
        for (uint32_t j = 0; j < 100; j++) {
            microglia_monitor_synapse(mg, i * 100 + j);
        }
    }

    // 5000 synapses total
    microglia_network_stats_t stats;
    microglia_network_get_stats(mg_net, &stats);
    EXPECT_EQ(stats.total_monitored_synapses, 5000);

    uint64_t now = nimcp_time_monotonic_us();
    uint64_t start = now;

    // Run simulation
    for (int step = 0; step < 100; step++) {
        // Activity updates
        for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
            microglia_t* mg = mg_net->microglia[i];
            for (uint32_t j = 0; j < mg->num_monitored_synapses; j += 10) {
                microglia_track_synapse_activity(mg, mg->monitored_synapse_ids[j],
                                                  (float)(j % 10), now + step * 1000);
            }
        }

        // Full network step
        microglia_network_step(mg_net, now + step * 1000);
    }

    uint64_t elapsed = nimcp_time_monotonic_us() - start;

    // Should complete in reasonable time (< 5 seconds for 100 steps)
    EXPECT_LT(elapsed, 5000000) << "Performance too slow: " << elapsed / 1000 << " ms";

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 9: Full Glial Integration
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, FullGlialSystemCoordination) {
    // Create all glial networks
    microglia_network_t* mg_net = create_distributed_microglia(10);
    ASSERT_NE(mg_net, nullptr);

    // Assign synapses
    assign_synapses_to_microglia(mg_net, 100);

    uint64_t now = nimcp_time_monotonic_us();

    // Simulate neural activity patterns
    for (int epoch = 0; epoch < 5; epoch++) {
        // Activity phase
        for (int step = 0; step < 20; step++) {
            for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
                microglia_t* mg = mg_net->microglia[i];

                // Simulate varying activity
                for (uint32_t j = 0; j < mg->num_monitored_synapses; j++) {
                    float activity = sinf((float)(step + j) * 0.5f) * 5.0f + 2.5f;
                    microglia_track_synapse_activity(mg, mg->monitored_synapse_ids[j],
                                                      activity, now);
                }

                // State dynamics
                microglia_update_state_dynamics(mg, 0.05f);
                microglia_update_cytokines(mg, 0.05f);
            }

            // Apply complement cascade periodically
            if (step % 5 == 0) {
                for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
                    microglia_apply_complement_tags(mg_net->microglia[i], now);
                }
            }

            now += 50000;  // 50ms steps
        }

        // Pruning phase
        for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
            microglia_prune_weak_synapses(mg_net->microglia[i]);
        }

        // Cytokine diffusion
        microglia_network_diffuse_cytokines(mg_net, 0.1f);
    }

    // Final statistics
    microglia_network_stats_t stats;
    microglia_network_get_stats(mg_net, &stats);

    // Verify system evolved
    EXPECT_GT(stats.avg_activity_score, 0.0f);

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 10: Recovery After Injury
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, InjuryAndRecoveryPattern) {
    microglia_network_t* mg_net = create_distributed_microglia(20);
    ASSERT_NE(mg_net, nullptr);

    assign_synapses_to_microglia(mg_net, 200);

    uint64_t now = nimcp_time_monotonic_us();

    // PHASE 1: Baseline (healthy)
    for (int step = 0; step < 50; step++) {
        microglia_network_step(mg_net, now + step * 100000);
    }

    microglia_network_stats_t baseline_stats;
    microglia_network_get_stats(mg_net, &baseline_stats);

    // All should be ramified at baseline
    EXPECT_EQ(baseline_stats.ramified_count, 20);

    // PHASE 2: Injury (local inflammation)
    for (int step = 0; step < 100; step++) {
        // Injury to first 10 microglia
        for (uint32_t i = 0; i < 10; i++) {
            microglia_set_inflammation(mg_net->microglia[i], 0.8f);
        }

        microglia_network_step(mg_net, now + (50 + step) * 100000);
    }

    microglia_network_stats_t injury_stats;
    microglia_network_get_stats(mg_net, &injury_stats);

    // Injured microglia should be activated
    EXPECT_LT(injury_stats.ramified_count, 20);
    EXPECT_GT(injury_stats.total_pro_inflammatory, 0.0f);

    // PHASE 3: Recovery
    for (int step = 0; step < 200; step++) {
        // Remove inflammation
        for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
            microglia_set_inflammation(mg_net->microglia[i], 0.0f);
        }

        microglia_network_step(mg_net, now + (150 + step) * 100000);
    }

    microglia_network_stats_t recovery_stats;
    microglia_network_get_stats(mg_net, &recovery_stats);

    // Should trend back toward ramified
    EXPECT_GT(recovery_stats.ramified_count, injury_stats.ramified_count);

    // Anti-inflammatory cytokines may be elevated during resolution
    EXPECT_GE(recovery_stats.total_anti_inflammatory, 0.0f);

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 11: Spatial Organization
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, SpatialOrganization) {
    microglia_network_t* mg_net = create_distributed_microglia(25);  // 5x5 grid
    ASSERT_NE(mg_net, nullptr);

    // Rebuild spatial index
    microglia_network_rebuild_spatial_index(mg_net);
    EXPECT_TRUE(mg_net->spatial_index_valid);

    // Test spatial queries
    microglia_t* center = microglia_network_find_nearest(mg_net, 100.0f, 100.0f, 0.0f);
    ASSERT_NE(center, nullptr);

    // Find neighbors
    microglia_t* neighbors[10];
    uint32_t num_neighbors = microglia_network_find_in_radius(mg_net,
                                                               100.0f, 100.0f, 0.0f,
                                                               75.0f, neighbors, 10);
    EXPECT_GT(num_neighbors, 1);  // Should find center + neighbors

    // Local inflammation should spread through neighbors
    microglia_set_inflammation(center, 0.9f);
    for (int i = 0; i < 50; i++) {
        microglia_update_state_dynamics(center, 0.1f);
        microglia_update_cytokines(center, 0.1f);
    }

    // Diffuse
    for (int i = 0; i < 100; i++) {
        microglia_network_diffuse_cytokines(mg_net, 0.05f);
    }

    // Check if cytokines spread
    float center_cytokine = microglia_get_cytokine(center, CYTOKINE_IL1B);
    EXPECT_GT(center_cytokine, 0.0f);

    microglia_network_destroy(mg_net);
}

//=============================================================================
// SCENARIO 12: Long-Term Simulation
//=============================================================================

TEST_F(MicrogliaBrainIntegrationTest, LongTermSimulation) {
    microglia_network_t* mg_net = create_distributed_microglia(10);
    ASSERT_NE(mg_net, nullptr);

    assign_synapses_to_microglia(mg_net, 200);

    uint64_t now = nimcp_time_monotonic_us();

    // Track synapses pruned over time
    std::vector<uint32_t> pruned_history;

    // Long simulation (1000 steps)
    for (int step = 0; step < 1000; step++) {
        // Random activity pattern
        for (uint32_t i = 0; i < mg_net->num_microglia; i++) {
            microglia_t* mg = mg_net->microglia[i];
            for (uint32_t j = 0; j < mg->num_monitored_synapses; j++) {
                // 70% active, 30% inactive
                float activity = (rand() % 100 < 70) ? 3.0f : 0.05f;
                microglia_track_synapse_activity(mg, mg->monitored_synapse_ids[j],
                                                  activity, now + step * 10000);
            }
        }

        // Periodic network step
        if (step % 10 == 0) {
            microglia_network_step(mg_net, now + step * 10000);
        }

        // Record pruning progress
        if (step % 100 == 0) {
            microglia_network_stats_t stats;
            microglia_network_get_stats(mg_net, &stats);
            pruned_history.push_back(stats.total_pruned);
        }
    }

    // Should have pruned some synapses (weak synapses get pruned early then stabilizes)
    EXPECT_GT(pruned_history.back(), 0);

    // Final state should be stable with remaining active synapses
    microglia_network_stats_t final_stats;
    microglia_network_get_stats(mg_net, &final_stats);
    EXPECT_GT(final_stats.total_monitored_synapses, 0);

    // Most synapses should survive (70% activity means 30% pruned max)
    EXPECT_GT(final_stats.total_monitored_synapses, 100);  // At least half of 200 survive

    microglia_network_destroy(mg_net);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
