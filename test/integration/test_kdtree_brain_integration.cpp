/**
 * @file test_kdtree_brain_integration.cpp
 * @brief Integration tests for KD-tree with brain spatial queries
 *
 * WHAT: Test KD-tree range search integrated with brain neuron placement
 * WHY:  Verify spatial indexing works correctly with neural structures
 * HOW:  Create brain, build KD-tree, perform spatial queries
 *
 * @author NIMCP Test Team
 * @date 2025-01-17
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "utils/spatial/nimcp_kdtree.h"
#include <vector>
#include <array>
#include <random>

//=============================================================================
// Test Fixture
//=============================================================================

class KDTreeBrainIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    kdtree_t* tree;

    void SetUp() override {
        brain = nullptr;
        tree = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
        if (tree) {
            kdtree_destroy(tree);
        }
    }
};

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(KDTreeBrainIntegrationTest, NeuronPlacementIndexing) {
    // WHAT: Index neuron positions in 3D space
    // WHY:  Biological neurons have spatial locations
    // HOW:  Simulate neuron positions, build KD-tree, query neighbors

    // Create simulated neuron positions (cortical columns)
    std::vector<std::array<float, 3>> neuron_positions;
    std::vector<void*> neuron_ids;

    // Create 100 neurons in a 10x10x1 grid (cortical sheet)
    for (int x = 0; x < 10; x++) {
        for (int y = 0; y < 10; y++) {
            std::array<float, 3> pos = {{
                (float)x * 0.1f,
                (float)y * 0.1f,
                0.0f
            }};
            neuron_positions.push_back(pos);
            neuron_ids.push_back((void*)(uintptr_t)(x * 10 + y));
        }
    }

    // Build KD-tree
    tree = kdtree_create();
    ASSERT_NE(tree, nullptr);

    bool success = kdtree_build(tree, reinterpret_cast<const kdtree_point_t*>(neuron_positions.data()),
                               neuron_ids.data(),
                               static_cast<uint32_t>(neuron_positions.size()));
    ASSERT_TRUE(success);

    // Query: Find all neurons within radius of center
    std::array<float, 3> query_point = {{0.5f, 0.5f, 0.0f}};  // Center of grid
    void* results[100];
    float radius = 0.2f;

    uint32_t count = kdtree_range_search(tree, query_point.data(), radius, results, 100);

    // Should find neurons near center
    EXPECT_GT(count, 0);
    EXPECT_LT(count, 100);  // Not all neurons

    // Verify all found neurons are actually within radius
    for (uint32_t i = 0; i < count; i++) {
        uint32_t neuron_id = (uintptr_t)results[i];
        ASSERT_LT(neuron_id, neuron_positions.size());

        const std::array<float, 3>& pos = neuron_positions[neuron_id];
        float dx = pos[0] - query_point[0];
        float dy = pos[1] - query_point[1];
        float dz = pos[2] - query_point[2];
        float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

        EXPECT_LE(distance, radius);
    }
}

TEST_F(KDTreeBrainIntegrationTest, SynapseFormation) {
    // WHAT: Use KD-tree to find nearby neurons for synapse formation
    // WHY:  Biological synapses form between nearby neurons
    // HOW:  Query range to find connection candidates

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // Create random neuron positions
    std::vector<std::array<float, 3>> neurons;
    std::vector<void*> neuron_data;

    for (int i = 0; i < 200; i++) {
        std::array<float, 3> pos = {{dist(rng), dist(rng), dist(rng)}};
        neurons.push_back(pos);
        neuron_data.push_back((void*)(uintptr_t)i);
    }

    // Build spatial index
    tree = kdtree_create();
    ASSERT_NE(tree, nullptr);

    bool success = kdtree_build(tree, reinterpret_cast<const kdtree_point_t*>(neurons.data()), neuron_data.data(),
                               static_cast<uint32_t>(neurons.size()));
    ASSERT_TRUE(success);

    // Form synapses: For each neuron, find nearby partners
    uint32_t total_synapses = 0;
    float connection_radius = 0.3f;  // Axon/dendrite reach

    for (size_t i = 0; i < neurons.size(); i++) {
        void* candidates[50];
        uint32_t num_candidates = kdtree_range_search(tree, neurons[i].data(),
                                                       connection_radius,
                                                       candidates, 50);

        // Each neuron should have some nearby partners (but not itself ideally)
        // In practice, itself will be found at distance 0
        EXPECT_GT(num_candidates, 0);

        total_synapses += num_candidates;
    }

    // Should have formed many synapses
    EXPECT_GT(total_synapses, 200);  // At least one per neuron
    EXPECT_LT(total_synapses, 10000);  // But not too many (sparse connectivity)

    std::cout << "Formed " << total_synapses << " potential synapses "
              << "(" << (total_synapses / 200.0f) << " per neuron)" << std::endl;
}

TEST_F(KDTreeBrainIntegrationTest, AstrocyteNetworkQuery) {
    // WHAT: Find astrocytes covering synapse locations
    // WHY:  Astrocytes regulate nearby synapses
    // HOW:  Synapse locations query for covering astrocytes

    // Create astrocyte tile positions (regular grid)
    std::vector<std::array<float, 3>> astrocyte_positions;
    std::vector<void*> astrocyte_ids;

    for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
            for (int z = 0; z < 5; z++) {
                std::array<float, 3> pos = {{
                    (float)x * 0.4f,
                    (float)y * 0.4f,
                    (float)z * 0.4f
                }};
                astrocyte_positions.push_back(pos);
                astrocyte_ids.push_back((void*)(uintptr_t)astrocyte_positions.size());
            }
        }
    }

    // Build astrocyte spatial index
    tree = kdtree_create();
    ASSERT_NE(tree, nullptr);

    bool success = kdtree_build(tree, reinterpret_cast<const kdtree_point_t*>(astrocyte_positions.data()),
                               astrocyte_ids.data(),
                               static_cast<uint32_t>(astrocyte_positions.size()));
    ASSERT_TRUE(success);

    // Simulate random synapse locations
    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(0.0f, 2.0f);

    uint32_t synapses_covered = 0;
    float astrocyte_reach = 0.5f;  // Astrocyte coverage radius

    for (int i = 0; i < 100; i++) {
        std::array<float, 3> synapse_location = {{dist(rng), dist(rng), dist(rng)}};

        void* covering_astrocytes[10];
        uint32_t num_covering = kdtree_range_search(tree, synapse_location.data(),
                                                     astrocyte_reach,
                                                     covering_astrocytes, 10);

        if (num_covering > 0) {
            synapses_covered++;
        }
    }

    // Most synapses should be covered by at least one astrocyte
    EXPECT_GT(synapses_covered, 80);

    std::cout << "Astrocyte coverage: " << synapses_covered << "/100 synapses"
              << " (" << synapses_covered << "%)" << std::endl;
}

TEST_F(KDTreeBrainIntegrationTest, BrainWithSpatialFeatures) {
    // WHAT: Integrate KD-tree queries with actual brain operations
    // WHY:  End-to-end test with real brain
    // HOW:  Create brain, simulate spatial queries during learning

    // Create brain (10 inputs to match feature vector size)
    brain = brain_create("spatial_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 20);
    ASSERT_NE(brain, nullptr);

    // Get brain stats to understand neuron count
    brain_stats_t stats;
    bool success = brain_get_stats(brain, &stats);
    ASSERT_TRUE(success);

    uint32_t num_neurons = stats.num_neurons;
    ASSERT_GT(num_neurons, 0);

    // Simulate neuron positions (we don't have direct access, so simulate)
    std::mt19937 rng(456);
    std::uniform_real_distribution<float> dist(-5.0f, 5.0f);

    std::vector<std::array<float, 3>> simulated_positions;
    std::vector<void*> simulated_ids;

    for (uint32_t i = 0; i < num_neurons; i++) {
        std::array<float, 3> pos = {{dist(rng), dist(rng), dist(rng)}};
        simulated_positions.push_back(pos);
        simulated_ids.push_back((void*)(uintptr_t)i);
    }

    // Build spatial index
    tree = kdtree_create();
    ASSERT_NE(tree, nullptr);

    success = kdtree_build(tree, reinterpret_cast<const kdtree_point_t*>(simulated_positions.data()),
                          simulated_ids.data(),
                          static_cast<uint32_t>(simulated_positions.size()));
    ASSERT_TRUE(success);

    // Perform spatial query (e.g., for local circuit analysis)
    std::array<float, 3> focus_point = {{0.0f, 0.0f, 0.0f}};
    void* local_neurons[100];
    float local_radius = 2.0f;

    uint32_t local_count = kdtree_range_search(tree, focus_point.data(),
                                               local_radius,
                                               local_neurons, 100);

    // Should find some neurons in local circuit
    EXPECT_GT(local_count, 0);
    EXPECT_LE(local_count, num_neurons);

    std::cout << "Local circuit: " << local_count << " neurons within radius "
              << local_radius << std::endl;

    // Train brain on simple task
    std::vector<float> features = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                                  0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    float loss = brain_learn_example(brain, features.data(),
                                     static_cast<uint32_t>(features.size()),
                                     "class_A", 1.0f);

    // Learning should succeed
    if (loss < 0.0f) {
        std::cerr << "brain_learn_example failed with loss = " << loss << std::endl;
        std::cerr << "Features size: " << features.size() << std::endl;
    }
    EXPECT_GE(loss, 0.0f);

    // After learning, spatial queries still work
    uint32_t new_count = kdtree_range_search(tree, focus_point.data(),
                                             local_radius,
                                             local_neurons, 100);

    // Count should be the same (neurons don't move)
    EXPECT_EQ(new_count, local_count);
}

//=============================================================================
// Performance Integration Tests
//=============================================================================

TEST_F(KDTreeBrainIntegrationTest, Performance_LargeScale) {
    // WHAT: Performance with realistic neuron counts
    // WHY:  Ensure scalability
    // HOW:  1000 neurons, many queries

    std::mt19937 rng(789);
    std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

    // Create 1000 neurons
    std::vector<std::array<float, 3>> neurons;
    std::vector<void*> neuron_ids;

    for (int i = 0; i < 1000; i++) {
        std::array<float, 3> pos = {{dist(rng), dist(rng), dist(rng)}};
        neurons.push_back(pos);
        neuron_ids.push_back((void*)(uintptr_t)i);
    }

    // Build KD-tree
    tree = kdtree_create();
    ASSERT_NE(tree, nullptr);

    auto build_start = std::chrono::high_resolution_clock::now();

    bool success = kdtree_build(tree, reinterpret_cast<const kdtree_point_t*>(neurons.data()), neuron_ids.data(),
                               static_cast<uint32_t>(neurons.size()));
    ASSERT_TRUE(success);

    auto build_end = std::chrono::high_resolution_clock::now();
    auto build_time = std::chrono::duration_cast<std::chrono::microseconds>(
        build_end - build_start).count();

    std::cout << "KD-tree build time: " << build_time << " us" << std::endl;

    // Perform 100 range queries
    void* results[100];
    float radius = 2.0f;

    auto query_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        std::array<float, 3> query = {{dist(rng), dist(rng), dist(rng)}};
        kdtree_range_search(tree, query.data(), radius, results, 100);
    }

    auto query_end = std::chrono::high_resolution_clock::now();
    auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(
        query_end - query_start).count();

    double avg_query_time = query_time / 100.0;

    std::cout << "Average query time: " << avg_query_time << " us" << std::endl;

    // Should be much faster than O(N) brute force
    EXPECT_LT(avg_query_time, 500.0);  // < 0.5ms per query
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
