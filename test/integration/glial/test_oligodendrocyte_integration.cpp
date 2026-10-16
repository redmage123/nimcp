/**
 * @file test_oligodendrocyte_integration.cpp
 * @brief Integration tests for Enhanced Oligodendrocyte Module
 *
 * Tests oligodendrocyte integration with:
 * - Network operations with multiple oligodendrocytes
 * - KD-tree spatial indexing performance
 * - Growth factor diffusion between cells
 * - Activity-driven myelination across network
 * - Centrality-based prioritization at scale
 * - Lactate shuttle metabolic coordination
 * - Concurrent access thread safety
 *
 * 12 comprehensive integration tests
 */

#include <gtest/gtest.h>
#include <cmath>
#include <thread>
#include <vector>
#include <chrono>
#include <random>

// Headers have their own extern "C" guards
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class OligodendrocyteIntegrationTest : public ::testing::Test {
protected:
    oligodendrocyte_network_t* network = nullptr;

    void SetUp() override {
        network = nullptr;
    }

    void TearDown() override {
        if (network) {
            oligodendrocyte_network_destroy(network);
            network = nullptr;
        }
    }

    // Helper: create network with N oligodendrocytes at random positions
    void createNetworkWithOligos(uint32_t count, float spacing) {
        network = oligodendrocyte_network_create(count + 10);
        ASSERT_NE(network, nullptr);

        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(0, spacing * count);

        for (uint32_t i = 0; i < count; i++) {
            float x = dist(gen);
            float y = dist(gen);
            float z = dist(gen);
            oligodendrocyte_t* oligo = oligodendrocyte_create(i, x, y, z, 20);
            ASSERT_NE(oligo, nullptr);
            oligodendrocyte_network_add(network, oligo);
        }
    }

    // Helper: assign axons to all oligodendrocytes
    void assignAxonsToAll(uint32_t axons_per_oligo) {
        uint32_t axon_id = 0;
        for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
            oligodendrocyte_t* oligo = network->oligodendrocytes[i];
            for (uint32_t j = 0; j < axons_per_oligo; j++) {
                oligodendrocyte_assign_neuron(oligo, axon_id++);
            }
        }
    }
};

//=============================================================================
// INTEGRATION TEST 1: Network with Multiple Oligodendrocytes
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, MultipleOligodendrocytesCoordinated) {
    createNetworkWithOligos(10, 100.0f);
    assignAxonsToAll(5);

    // Generate activity on all axons
    uint64_t timestamp = nimcp_time_monotonic_us();
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
            oligodendrocyte_track_activity(oligo, oligo->axons[j].axon_id, 10.0f, timestamp + j * 100);
        }
    }

    // Run network simulation
    for (int step = 0; step < 100; step++) {
        oligodendrocyte_network_step(network, 0.1f);
    }

    // Verify all oligodendrocytes have processed
    oligodendrocyte_network_stats_t stats;
    oligodendrocyte_network_get_stats(network, &stats);

    EXPECT_EQ(stats.total_oligodendrocytes, 10);
    EXPECT_EQ(stats.total_myelinated_axons, 50);
    EXPECT_GT(stats.avg_myelination_level, 0.0f);
}

//=============================================================================
// INTEGRATION TEST 2: Spatial Indexing with KD-Tree
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, SpatialIndexingPerformance) {
    createNetworkWithOligos(100, 1000.0f);

    // Build spatial index
    oligodendrocyte_network_rebuild_spatial_index(network);
    ASSERT_TRUE(network->spatial_index_valid);

    // Perform many nearest-neighbor queries
    std::mt19937 gen(123);
    std::uniform_real_distribution<float> dist(0, 100000.0f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        float x = dist(gen);
        float y = dist(gen);
        float z = dist(gen);
        oligodendrocyte_t* nearest = oligodendrocyte_network_find_nearest(network, x, y, z);
        ASSERT_NE(nearest, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 1000 queries should complete in under 100ms with KD-tree
    EXPECT_LT(duration, 100);
}

//=============================================================================
// INTEGRATION TEST 3: Growth Factor Diffusion
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, GrowthFactorDiffusion) {
    // Create two oligodendrocytes close together
    network = oligodendrocyte_network_create(10);

    oligodendrocyte_t* o1 = oligodendrocyte_create(1, 0, 0, 0, 20);
    oligodendrocyte_t* o2 = oligodendrocyte_create(2, 50, 0, 0, 20);  // Within diffusion radius

    oligodendrocyte_network_add(network, o1);
    oligodendrocyte_network_add(network, o2);

    // Add growth factor to o1 only
    oligodendrocyte_add_growth_factor(o1, GROWTH_FACTOR_NRG1, 8.0f);
    float initial_o2 = oligodendrocyte_get_growth_factor(o2, GROWTH_FACTOR_NRG1);

    // Run diffusion
    for (int i = 0; i < 100; i++) {
        oligodendrocyte_network_step(network, 0.1f);
    }

    float final_o2 = oligodendrocyte_get_growth_factor(o2, GROWTH_FACTOR_NRG1);

    // o2 should receive some growth factor from o1
    EXPECT_GT(final_o2, initial_o2);
}

//=============================================================================
// INTEGRATION TEST 4: Activity-Driven Myelination Across Network
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, ActivityDrivenMyelination) {
    createNetworkWithOligos(5, 100.0f);
    assignAxonsToAll(10);

    // Different activity levels for different axons
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
            float activity = (j < 5) ? 20.0f : 2.0f;  // High vs low activity
            for (int k = 0; k < 20; k++) {
                oligodendrocyte_track_activity(oligo, oligo->axons[j].axon_id, activity,
                                               nimcp_time_monotonic_us() + k * 1000);
            }
        }
    }

    // Run simulation
    for (int step = 0; step < 200; step++) {
        oligodendrocyte_network_step(network, 0.1f);
    }

    // High-activity axons should be more myelinated
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];

        float high_activity_myelin = 0.0f;
        float low_activity_myelin = 0.0f;

        for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
            if (j < 5) {
                high_activity_myelin += oligo->axons[j].myelination_level;
            } else {
                low_activity_myelin += oligo->axons[j].myelination_level;
            }
        }

        EXPECT_GT(high_activity_myelin, low_activity_myelin);
    }
}

//=============================================================================
// INTEGRATION TEST 5: Centrality-Based Prioritization at Scale
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, CentralityPrioritizationScale) {
    createNetworkWithOligos(10, 100.0f);
    assignAxonsToAll(10);

    // Set centrality scores
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
            float centrality = (j % 2 == 0) ? 0.9f : 0.1f;
            oligodendrocyte_set_axon_centrality(oligo, oligo->axons[j].axon_id, centrality);
        }
    }

    // Equal activity for all
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
            for (int k = 0; k < 10; k++) {
                oligodendrocyte_track_activity(oligo, oligo->axons[j].axon_id, 5.0f,
                                               nimcp_time_monotonic_us() + k * 1000);
            }
        }
    }

    // Run simulation
    for (int step = 0; step < 200; step++) {
        oligodendrocyte_network_step(network, 0.1f);
    }

    // High-centrality axons should have more myelination
    uint32_t high_wins = 0;
    uint32_t comparisons = 0;

    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < oligo->num_myelinated_axons; j += 2) {
            if (j + 1 < oligo->num_myelinated_axons) {
                if (oligo->axons[j].myelination_level > oligo->axons[j+1].myelination_level) {
                    high_wins++;
                }
                comparisons++;
            }
        }
    }

    // High-centrality should win majority of comparisons
    EXPECT_GT(high_wins, comparisons / 2);
}

//=============================================================================
// INTEGRATION TEST 6: Lactate Shuttle Metabolic Coordination
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, LactateShuttleCoordination) {
    createNetworkWithOligos(5, 100.0f);
    assignAxonsToAll(5);

    // Set varying metabolic demands
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
            float demand = 1.0f + j * 0.5f;  // Increasing demand
            oligodendrocyte_set_axon_demand(oligo, oligo->axons[j].axon_id, demand);
        }
    }

    // Run simulation
    for (int step = 0; step < 100; step++) {
        oligodendrocyte_network_step(network, 0.1f);
    }

    // Verify lactate was delivered
    oligodendrocyte_network_stats_t stats;
    oligodendrocyte_network_get_stats(network, &stats);
    EXPECT_GT(stats.total_lactate_delivered, 0.0f);
}

//=============================================================================
// INTEGRATION TEST 7: G-Ratio Optimization Convergence
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, GRatioConvergence) {
    createNetworkWithOligos(5, 100.0f);

    // Assign axons with varying diameters
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < 5; j++) {
            float diameter = 1.0f + j * 1.0f;  // 1-5 µm
            oligodendrocyte_assign_axon_at(oligo, i * 10 + j, 0, 0, 0, diameter, 500.0f);
            oligodendrocyte_set_myelination_level(oligo, i * 10 + j, 0.8f);
            // Track activity to maintain myelination while G-ratio optimizes
            oligodendrocyte_track_activity(oligo, i * 10 + j, 10.0f, nimcp_time_monotonic_us());
        }
    }

    // Run optimization
    float initial_deviation = 0.0f;
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        initial_deviation += oligodendrocyte_get_g_ratio_deviation(network->oligodendrocytes[i]);
    }

    for (int step = 0; step < 500; step++) {
        // Periodically refresh activity to maintain myelination
        if (step % 50 == 0) {
            for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
                oligodendrocyte_t* oligo = network->oligodendrocytes[i];
                for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
                    oligodendrocyte_track_activity(oligo, oligo->axons[j].axon_id, 10.0f,
                                                   nimcp_time_monotonic_us());
                }
            }
        }
        oligodendrocyte_network_step(network, 0.1f);
    }

    float final_deviation = 0.0f;
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        final_deviation += oligodendrocyte_get_g_ratio_deviation(network->oligodendrocytes[i]);
    }

    // Deviation should decrease (better optimization)
    EXPECT_LE(final_deviation, initial_deviation);
}

//=============================================================================
// INTEGRATION TEST 8: Network Statistics Accuracy
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, NetworkStatisticsAccuracy) {
    network = oligodendrocyte_network_create(20);

    // Create oligodendrocytes at different maturation states
    for (uint32_t i = 0; i < 4; i++) {
        oligodendrocyte_t* oligo = oligodendrocyte_create(i, i * 100.0f, 0, 0, 10);
        for (uint32_t j = 0; j < i; j++) {
            oligodendrocyte_advance_maturation(oligo);
        }
        oligodendrocyte_assign_neuron(oligo, i);
        oligodendrocyte_network_add(network, oligo);
    }

    oligodendrocyte_network_stats_t stats;
    oligodendrocyte_network_get_stats(network, &stats);

    EXPECT_EQ(stats.total_oligodendrocytes, 4);
    EXPECT_EQ(stats.opc_count, 1);
    EXPECT_EQ(stats.pre_ol_count, 1);
    EXPECT_EQ(stats.immature_count, 1);
    EXPECT_EQ(stats.mature_count, 1);
}

//=============================================================================
// INTEGRATION TEST 9: Conduction Velocity Distribution
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, ConductionVelocityDistribution) {
    createNetworkWithOligos(5, 100.0f);

    // Assign axons with different myelination levels
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < 5; j++) {
            oligodendrocyte_assign_axon_at(oligo, i * 10 + j, 0, 0, 0, 2.0f, 500.0f);
            float myelin_level = 0.2f * j;  // 0, 0.2, 0.4, 0.6, 0.8
            oligodendrocyte_set_myelination_level(oligo, i * 10 + j, myelin_level);
        }
    }

    // Trigger velocity computation
    for (int step = 0; step < 10; step++) {
        oligodendrocyte_network_step(network, 0.1f);
    }

    oligodendrocyte_network_stats_t stats;
    oligodendrocyte_network_get_stats(network, &stats);

    // Verify velocity range
    EXPECT_GE(stats.min_conduction_velocity, NIMCP_OLIGO_BASE_VELOCITY_MS);
    EXPECT_LE(stats.max_conduction_velocity, NIMCP_OLIGO_BASE_VELOCITY_MS * NIMCP_OLIGO_MYELIN_MULTIPLIER);
    EXPECT_LT(stats.min_conduction_velocity, stats.max_conduction_velocity);
}

//=============================================================================
// INTEGRATION TEST 10: Concurrent Network Access (Thread Safety)
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, ConcurrentNetworkAccess) {
    createNetworkWithOligos(10, 100.0f);
    assignAxonsToAll(5);

    std::atomic<int> completed{0};

    // Thread 1: Network steps
    std::thread stepper([this, &completed]() {
        for (int i = 0; i < 50; i++) {
            oligodendrocyte_network_step(network, 0.01f);
        }
        completed++;
    });

    // Thread 2: Activity tracking
    std::thread tracker([this, &completed]() {
        for (int i = 0; i < 100; i++) {
            for (uint32_t j = 0; j < network->num_oligodendrocytes; j++) {
                oligodendrocyte_t* oligo = network->oligodendrocytes[j];
                if (oligo && oligo->num_myelinated_axons > 0) {
                    oligodendrocyte_track_activity(oligo, oligo->axons[0].axon_id, 5.0f,
                                                   nimcp_time_monotonic_us());
                }
            }
        }
        completed++;
    });

    // Thread 3: Stats queries
    std::thread stats_query([this, &completed]() {
        for (int i = 0; i < 50; i++) {
            oligodendrocyte_network_stats_t stats;
            oligodendrocyte_network_get_stats(network, &stats);
        }
        completed++;
    });

    stepper.join();
    tracker.join();
    stats_query.join();

    EXPECT_EQ(completed.load(), 3);
}

//=============================================================================
// INTEGRATION TEST 11: Full Simulation Cycle
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, FullSimulationCycle) {
    createNetworkWithOligos(20, 200.0f);
    assignAxonsToAll(10);
    oligodendrocyte_network_rebuild_spatial_index(network);

    // Add growth factors
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_add_growth_factor(network->oligodendrocytes[i], GROWTH_FACTOR_NRG1, 3.0f);
        oligodendrocyte_add_growth_factor(network->oligodendrocytes[i], GROWTH_FACTOR_BDNF, 2.0f);
    }

    // Set centrality
    for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
            float centrality = (float)j / (float)oligo->num_myelinated_axons;
            oligodendrocyte_set_axon_centrality(oligo, oligo->axons[j].axon_id, centrality);
        }
    }

    // Simulate 10 seconds of real time
    float total_time = 0.0f;
    while (total_time < 10.0f) {
        // Update activity
        for (uint32_t i = 0; i < network->num_oligodendrocytes; i++) {
            oligodendrocyte_t* oligo = network->oligodendrocytes[i];
            for (uint32_t j = 0; j < oligo->num_myelinated_axons; j++) {
                float activity = 5.0f + 5.0f * sin(total_time + j);
                oligodendrocyte_track_activity(oligo, oligo->axons[j].axon_id, activity,
                                               (uint64_t)(total_time * 1e6));
            }
        }

        oligodendrocyte_network_step(network, 0.1f);
        total_time += 0.1f;
    }

    // Verify simulation produced reasonable results
    oligodendrocyte_network_stats_t stats;
    oligodendrocyte_network_get_stats(network, &stats);

    EXPECT_EQ(stats.total_oligodendrocytes, 20);
    EXPECT_EQ(stats.total_myelinated_axons, 200);
    EXPECT_GT(stats.avg_myelination_level, 0.0f);
    EXPECT_GT(stats.avg_conduction_velocity, NIMCP_OLIGO_BASE_VELOCITY_MS);
    EXPECT_GT(stats.total_lactate_delivered, 0.0f);
}

//=============================================================================
// INTEGRATION TEST 12: Radius Search Performance
//=============================================================================

TEST_F(OligodendrocyteIntegrationTest, RadiusSearchPerformance) {
    createNetworkWithOligos(100, 1000.0f);
    oligodendrocyte_network_rebuild_spatial_index(network);

    oligodendrocyte_t* results[50];
    uint32_t total_found = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        uint32_t count = oligodendrocyte_network_find_in_radius(network,
                                                                 500000.0f, 500000.0f, 500000.0f,
                                                                 200000.0f, results, 50);
        total_found += count;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete quickly
    EXPECT_LT(duration, 500);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
