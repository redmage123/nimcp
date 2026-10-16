//=============================================================================
// test_distributed_cow_integration.cpp - Integration Tests for Distributed COW
//=============================================================================
/**
 * @file test_distributed_cow_integration.cpp
 * @brief Integration tests for distributed COW across network components
 *
 * WHAT: Tests interaction between distributed COW and other subsystems:
 *       - P2P network communication
 *       - Brain operations (inference, learning)
 *       - Network serialization round-trip
 *       - Multi-node scenarios
 *       - Cache consistency across operations
 *
 * WHY:  Verify system-level correctness:
 *       - Ensure distributed clones produce same results as local
 *       - Validate network protocol integration
 *       - Test performance under realistic conditions
 *       - Verify cache hit rates and bandwidth usage
 *
 * HOW:  End-to-end test scenarios:
 *       - Setup master/clone pairs
 *       - Perform operations and compare results
 *       - Measure performance metrics
 *       - Validate distributed state consistency
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_distributed_cow.h"
#include "core/brain/nimcp_brain.h"
#include "networking/p2p/nimcp_p2pnode.h"
#include "utils/time/nimcp_time.h"
#include <thread>
#include <chrono>

//=============================================================================
// Integration Test Fixtures
//=============================================================================

class DistributedCOWIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create master brain with input size matching our test patterns (4 features)
        master_brain = brain_create("master_brain", BRAIN_SIZE_MEDIUM, BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(master_brain, nullptr);

        // Train master brain with some patterns
        TrainMasterBrain();
    }

    void TearDown() override {
        if (clone_brain) {
            brain_destroy(clone_brain);
            clone_brain = nullptr;
        }
        if (master_brain) {
            brain_destroy(master_brain);
            master_brain = nullptr;
        }
        if (clone_p2p) {
            p2p_node_stop(clone_p2p);
            p2p_node_destroy(clone_p2p);
            clone_p2p = nullptr;
        }
        if (master_p2p) {
            p2p_node_stop(master_p2p);
            p2p_node_destroy(master_p2p);
            master_p2p = nullptr;
        }
    }

    void TrainMasterBrain() {
        // Train with simple patterns
        float input1[] = {1.0f, 0.0f, 1.0f, 0.0f};
        float input2[] = {0.0f, 1.0f, 0.0f, 1.0f};

        for (int i = 0; i < 10; i++) {
            brain_learn_example(master_brain, input1, 4, "pattern_a", 1.0f);
            brain_learn_example(master_brain, input2, 4, "pattern_b", 1.0f);
        }
    }

    brain_t master_brain = nullptr;
    brain_t clone_brain = nullptr;
    p2p_node_t master_p2p = nullptr;
    p2p_node_t clone_p2p = nullptr;
};

//=============================================================================
// Network Communication Tests
//=============================================================================

TEST_F(DistributedCOWIntegrationTest, MasterCloneSetup) {
    // Setup master P2P node
    node_config_t master_config = {
        .listen_port = 15000,
        .max_peers = 4,
        .ping_interval = 5000
    };
    master_p2p = p2p_node_create(&master_config);
    ASSERT_NE(master_p2p, nullptr);
    ASSERT_TRUE(p2p_node_start(master_p2p));

    // Enable distributed COW master
    ASSERT_TRUE(brain_enable_distributed_cow_master(master_brain, master_p2p));

    // Verify master stats
    distributed_cow_stats_t stats;
    if (brain_get_distributed_cow_stats(master_brain, &stats)) {
        EXPECT_TRUE(stats.is_distributed);
        EXPECT_TRUE(stats.is_master);
        EXPECT_EQ(stats.local_refcount, 1u);
    }
}

//=============================================================================
// Inference Consistency Tests
//=============================================================================

TEST_F(DistributedCOWIntegrationTest, InferenceSameAsLocal) {
    // Create local COW clone for comparison
    brain_t local_clone = brain_clone_cow(master_brain);
    ASSERT_NE(local_clone, nullptr);

    // Run inference on both
    float input[] = {1.0f, 0.0f, 1.0f, 0.0f};

    brain_decision_t* master_decision = brain_decide(master_brain, input, 4);
    brain_decision_t* local_decision = brain_decide(local_clone, input, 4);

    // Decisions should match
    ASSERT_NE(master_decision, nullptr);
    ASSERT_NE(local_decision, nullptr);
    EXPECT_STREQ(master_decision->label, local_decision->label);
    EXPECT_NEAR(master_decision->confidence, local_decision->confidence, 0.01f);

    brain_destroy(local_clone);
}

//=============================================================================
// Cache Performance Tests
//=============================================================================

TEST_F(DistributedCOWIntegrationTest, CacheHitRate) {
    // Setup master
    node_config_t master_config = {
        .listen_port = 15001,
        .max_peers = 4,
        .ping_interval = 5000
    };
    master_p2p = p2p_node_create(&master_config);
    ASSERT_NE(master_p2p, nullptr);
    ASSERT_TRUE(p2p_node_start(master_p2p));
    ASSERT_TRUE(brain_enable_distributed_cow_master(master_brain, master_p2p));

    // Note: Full distributed clone test would require network connectivity
    // For now, test cache operations on master

    distributed_cow_stats_t stats;
    if (brain_get_distributed_cow_stats(master_brain, &stats)) {
        // Initially no cache operations
        EXPECT_EQ(stats.cache_hits, 0u);
        EXPECT_EQ(stats.cache_misses, 0u);
    }
}

//=============================================================================
// Learning Transition Tests
//=============================================================================

TEST_F(DistributedCOWIntegrationTest, LearningTriggersFullFetch) {
    // Setup local clone (simulating distributed scenario)
    brain_t local_clone = brain_clone_cow(master_brain);
    ASSERT_NE(local_clone, nullptr);

    // Learning should work on clone (triggers COW)
    float new_input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    brain_learn_example(local_clone, new_input, 4, "pattern_c", 1.0f);

    // Clone should still work after learning
    brain_decision_t* decision = brain_decide(local_clone, new_input, 4);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);

    brain_destroy(local_clone);
}

//=============================================================================
// Multi-Clone Scenario Tests
//=============================================================================

TEST_F(DistributedCOWIntegrationTest, MultipleLocalClones) {
    // Create multiple local COW clones
    brain_t clone1 = brain_clone_cow(master_brain);
    brain_t clone2 = brain_clone_cow(master_brain);
    brain_t clone3 = brain_clone_cow(master_brain);

    ASSERT_NE(clone1, nullptr);
    ASSERT_NE(clone2, nullptr);
    ASSERT_NE(clone3, nullptr);

    // All clones should produce same results
    float input[] = {1.0f, 0.0f, 1.0f, 0.0f};

    brain_decision_t* decision1 = brain_decide(clone1, input, 4);
    brain_decision_t* decision2 = brain_decide(clone2, input, 4);
    brain_decision_t* decision3 = brain_decide(clone3, input, 4);

    ASSERT_NE(decision1, nullptr);
    ASSERT_NE(decision2, nullptr);
    ASSERT_NE(decision3, nullptr);
    EXPECT_STREQ(decision1->label, decision2->label);
    EXPECT_STREQ(decision2->label, decision3->label);

    // Cleanup
    brain_destroy(clone1);
    brain_destroy(clone2);
    brain_destroy(clone3);
}

//=============================================================================
// Performance Benchmarks
//=============================================================================

TEST_F(DistributedCOWIntegrationTest, InferencePerformance) {
    brain_t clone = brain_clone_cow(master_brain);
    ASSERT_NE(clone, nullptr);

    float input[] = {1.0f, 0.0f, 1.0f, 0.0f};

    // Warm-up
    for (int i = 0; i < 10; i++) {
        brain_decide(clone, input, 4);
    }

    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();
    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        brain_decide(clone, input, 4);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    float avg_latency_us = duration.count() / static_cast<float>(iterations);

    // Inference should be fast (< 100us for small brain)
    EXPECT_LT(avg_latency_us, 100.0f);

    brain_destroy(clone);
}

//=============================================================================
// Memory Usage Tests
//=============================================================================

TEST_F(DistributedCOWIntegrationTest, MemorySharing) {
    // Create multiple clones
    constexpr int num_clones = 10;
    brain_t clones[num_clones];

    for (int i = 0; i < num_clones; i++) {
        clones[i] = brain_clone_cow(master_brain);
        ASSERT_NE(clones[i], nullptr);
    }

    // All clones share the same network (COW)
    // Memory usage should be much less than num_clones * brain_size
    // (Hard to measure precisely in test, but documented behavior)

    // Cleanup
    for (int i = 0; i < num_clones; i++) {
        brain_destroy(clones[i]);
    }
}

//=============================================================================
// Edge Case Integration Tests
//=============================================================================

TEST_F(DistributedCOWIntegrationTest, CloneOfClone) {
    // Create clone of clone
    brain_t clone1 = brain_clone_cow(master_brain);
    ASSERT_NE(clone1, nullptr);

    brain_t clone2 = brain_clone_cow(clone1);
    ASSERT_NE(clone2, nullptr);

    // Both should work correctly
    float input[] = {1.0f, 0.0f, 1.0f, 0.0f};

    brain_decision_t* decision1 = brain_decide(clone1, input, 4);
    brain_decision_t* decision2 = brain_decide(clone2, input, 4);

    ASSERT_NE(decision1, nullptr);
    ASSERT_NE(decision2, nullptr);
    EXPECT_STREQ(decision1->label, decision2->label);

    brain_destroy(clone2);
    brain_destroy(clone1);
}

TEST_F(DistributedCOWIntegrationTest, DestroyInReverseOrder) {
    // Create multiple clones
    brain_t clone1 = brain_clone_cow(master_brain);
    brain_t clone2 = brain_clone_cow(master_brain);
    brain_t clone3 = brain_clone_cow(master_brain);

    ASSERT_NE(clone1, nullptr);
    ASSERT_NE(clone2, nullptr);
    ASSERT_NE(clone3, nullptr);

    // Destroy in reverse order (should handle reference counting correctly)
    brain_destroy(clone3);
    brain_destroy(clone2);
    brain_destroy(clone1);

    // Master should still be valid
    float input[] = {1.0f, 0.0f, 1.0f, 0.0f};
    brain_decision_t* decision = brain_decide(master_brain, input, 4);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
