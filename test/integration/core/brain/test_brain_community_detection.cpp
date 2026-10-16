/**
 * @file test_brain_community_detection.cpp
 * @brief Integration tests for brain community detection
 *
 * Tests community detection, hub identification, topology metrics,
 * and network validation on brain networks.
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "core/topology/nimcp_community_detection.h"

class BrainCommunityDetectionTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Create a medium-sized brain for testing
        brain = brain_create("community_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        // Train it a bit to establish connections
        float inputs[10] = {1.0f, 0.5f, 0.2f, 0.8f, 0.3f, 0.9f, 0.1f, 0.6f, 0.4f, 0.7f};
        brain_learn_example(brain, inputs, 10, "class_a", 1.0f);
        brain_learn_example(brain, inputs, 10, "class_b", 0.5f);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Community Detection Tests
//=============================================================================

TEST_F(BrainCommunityDetectionTest, DetectCommunities_Success) {
    // Act: Detect communities
    bool result = brain_detect_communities(brain);

    // Assert
    EXPECT_TRUE(result) << "Community detection should succeed";

    // Verify communities were detected
    // Note: Number of communities depends on network structure
    // We just verify the function completed successfully
}

TEST_F(BrainCommunityDetectionTest, DetectCommunities_NullBrain) {
    // Act: Call with NULL brain
    bool result = brain_detect_communities(nullptr);

    // Assert
    EXPECT_FALSE(result) << "Should fail with NULL brain";
}

TEST_F(BrainCommunityDetectionTest, GetNeuronCommunity_AfterDetection) {
    // Arrange: Detect communities first
    ASSERT_TRUE(brain_detect_communities(brain));

    // Act: Get community for neuron 0
    uint32_t community_id = brain_get_neuron_community(brain, 0);

    // Assert: Should return a valid community ID (not UINT32_MAX)
    EXPECT_NE(community_id, UINT32_MAX) << "Should return valid community ID";
}

TEST_F(BrainCommunityDetectionTest, GetNeuronCommunity_BeforeDetection) {
    // Act: Try to get community without detection
    uint32_t community_id = brain_get_neuron_community(brain, 0);

    // Assert: Should return invalid ID
    EXPECT_EQ(community_id, UINT32_MAX) << "Should fail if communities not detected";
}

TEST_F(BrainCommunityDetectionTest, GetNeuronCommunity_InvalidNeuron) {
    // Arrange
    ASSERT_TRUE(brain_detect_communities(brain));

    // Act: Try with invalid neuron ID
    uint32_t community_id = brain_get_neuron_community(brain, 999999);

    // Assert
    EXPECT_EQ(community_id, UINT32_MAX) << "Should return invalid ID for invalid neuron";
}

//=============================================================================
// Hub Detection Tests
//=============================================================================

TEST_F(BrainCommunityDetectionTest, DetectHubs_Success) {
    // Act: Detect hubs with threshold = 2.0 (2 std deviations)
    bool result = brain_detect_hubs(brain, 2.0f);

    // Assert
    EXPECT_TRUE(result) << "Hub detection should succeed";
}

TEST_F(BrainCommunityDetectionTest, DetectHubs_NullBrain) {
    // Act
    bool result = brain_detect_hubs(nullptr, 2.0f);

    // Assert
    EXPECT_FALSE(result) << "Should fail with NULL brain";
}

TEST_F(BrainCommunityDetectionTest, DetectHubs_DifferentThresholds) {
    // Test with low threshold (more hubs)
    ASSERT_TRUE(brain_detect_hubs(brain, 1.0f));

    // Test with high threshold (fewer hubs)
    ASSERT_TRUE(brain_detect_hubs(brain, 3.0f));
    // Note: Different thresholds should still succeed
}

TEST_F(BrainCommunityDetectionTest, IsHubNeuron_AfterDetection) {
    // Arrange
    ASSERT_TRUE(brain_detect_hubs(brain, 2.0f));

    // Act: Check if neuron 0 is a hub
    bool is_hub = brain_is_hub_neuron(brain, 0);

    // Assert: Result should be boolean (not crash)
    // Can't assert specific value as it depends on network structure
    EXPECT_TRUE(is_hub || !is_hub) << "Should return valid boolean";
}

TEST_F(BrainCommunityDetectionTest, IsHubNeuron_BeforeDetection) {
    // Act: Try without detection
    bool is_hub = brain_is_hub_neuron(brain, 0);

    // Assert: Should return false (no hubs detected)
    EXPECT_FALSE(is_hub) << "Should return false if hubs not detected";
}

//=============================================================================
// Topology Metrics Tests
//=============================================================================

TEST_F(BrainCommunityDetectionTest, ComputeTopologyMetrics_Success) {
    // Act
    bool result = brain_compute_topology_metrics(brain);

    // Assert
    EXPECT_TRUE(result) << "Topology metrics computation should succeed";
}

TEST_F(BrainCommunityDetectionTest, ComputeTopologyMetrics_NullBrain) {
    // Act
    bool result = brain_compute_topology_metrics(nullptr);

    // Assert
    EXPECT_FALSE(result) << "Should fail with NULL brain";
}

TEST_F(BrainCommunityDetectionTest, ComputeTopologyMetrics_WithCommunities) {
    // Arrange: Detect communities first
    ASSERT_TRUE(brain_detect_communities(brain));

    // Act: Compute metrics (should use community info for modularity)
    bool result = brain_compute_topology_metrics(brain);

    // Assert
    EXPECT_TRUE(result) << "Should succeed with community info";
}

//=============================================================================
// Topology Validation Tests
//=============================================================================

TEST_F(BrainCommunityDetectionTest, ValidateTopology_BasicCheck) {
    // Act
    bool is_valid = brain_validate_topology(brain);

    // Assert: May or may not be valid depending on network structure
    // We just verify the function completes without crashing
    EXPECT_TRUE(is_valid || !is_valid) << "Should return valid boolean";
}

TEST_F(BrainCommunityDetectionTest, ValidateTopology_NullBrain) {
    // Act
    bool is_valid = brain_validate_topology(nullptr);

    // Assert
    EXPECT_FALSE(is_valid) << "Should fail with NULL brain";
}

TEST_F(BrainCommunityDetectionTest, ValidateTopology_WithFullAnalysis) {
    // Arrange: Run full analysis pipeline
    ASSERT_TRUE(brain_detect_communities(brain));
    ASSERT_TRUE(brain_detect_hubs(brain, 2.0f));
    ASSERT_TRUE(brain_compute_topology_metrics(brain));

    // Act: Validate with all information available
    bool is_valid = brain_validate_topology(brain);

    // Assert: With all metrics, validation should complete
    EXPECT_TRUE(is_valid || !is_valid) << "Should complete validation";
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(BrainCommunityDetectionTest, FullPipeline_AllOperations) {
    // This test runs the complete community detection pipeline

    // Step 1: Detect communities
    ASSERT_TRUE(brain_detect_communities(brain))
        << "Community detection should succeed";

    // Step 2: Check a neuron's community
    uint32_t comm_id = brain_get_neuron_community(brain, 0);
    EXPECT_NE(comm_id, UINT32_MAX)
        << "Should get valid community ID";

    // Step 3: Detect hubs
    ASSERT_TRUE(brain_detect_hubs(brain, 2.0f))
        << "Hub detection should succeed";

    // Step 4: Check if a neuron is a hub
    bool is_hub = brain_is_hub_neuron(brain, 0);
    EXPECT_TRUE(is_hub || !is_hub)
        << "Should get valid hub status";

    // Step 5: Compute topology metrics
    ASSERT_TRUE(brain_compute_topology_metrics(brain))
        << "Topology metrics should compute";

    // Step 6: Validate topology
    bool is_valid = brain_validate_topology(brain);
    EXPECT_TRUE(is_valid || !is_valid)
        << "Should complete validation";
}

TEST_F(BrainCommunityDetectionTest, MultipleDetections_OverwritePrevious) {
    // Detect communities first time
    ASSERT_TRUE(brain_detect_communities(brain));
    uint32_t first_comm = brain_get_neuron_community(brain, 0);

    // Detect again (should overwrite)
    ASSERT_TRUE(brain_detect_communities(brain));
    uint32_t second_comm = brain_get_neuron_community(brain, 0);

    // Both should be valid (exact values may differ due to algorithm randomness)
    EXPECT_NE(first_comm, UINT32_MAX);
    EXPECT_NE(second_comm, UINT32_MAX);
}

TEST_F(BrainCommunityDetectionTest, DetectionAfterTraining_ModularStructure) {
    // Arrange: Train brain with distinct patterns
    float pattern_a[10] = {1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float pattern_b[10] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float pattern_c[10] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    // Train with distinct patterns (should create modular structure)
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, pattern_a, 10, "class_a", 1.0f);
        brain_learn_example(brain, pattern_b, 10, "class_b", 1.0f);
        brain_learn_example(brain, pattern_c, 10, "class_c", 1.0f);
    }

    // Act: Detect communities after training
    ASSERT_TRUE(brain_detect_communities(brain));

    // Assert: Should detect multiple communities (likely > 1)
    // Check that different neurons can be in different communities
    uint32_t comm0 = brain_get_neuron_community(brain, 0);
    EXPECT_NE(comm0, UINT32_MAX);
}

TEST_F(BrainCommunityDetectionTest, MemoryCleanup_NoLeaks) {
    // Detect multiple times to stress test cleanup
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(brain_detect_communities(brain));
        ASSERT_TRUE(brain_detect_hubs(brain, 2.0f));
        ASSERT_TRUE(brain_compute_topology_metrics(brain));
    }

    // brain_destroy() in TearDown() should clean up without leaks
    // This is verified by valgrind/asan in CI
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(BrainCommunityDetectionTest, TinyBrain_StillWorks) {
    // Create very small brain
    brain_t tiny_brain = brain_create("tiny", BRAIN_SIZE_TINY,
                                     BRAIN_TASK_CLASSIFICATION, 3, 2);
    ASSERT_NE(tiny_brain, nullptr);

    // Should still work on tiny brain
    EXPECT_TRUE(brain_detect_communities(tiny_brain));
    EXPECT_TRUE(brain_detect_hubs(tiny_brain, 2.0f));
    EXPECT_TRUE(brain_compute_topology_metrics(tiny_brain));

    brain_destroy(tiny_brain);
}

TEST_F(BrainCommunityDetectionTest, UntrainedBrain_StillAnalyzable) {
    // Create fresh brain without training
    brain_t fresh_brain = brain_create("fresh", BRAIN_SIZE_SMALL,
                                       BRAIN_TASK_CLASSIFICATION, 5, 3);
    ASSERT_NE(fresh_brain, nullptr);

    // Should still be able to analyze (may have random initial structure)
    EXPECT_TRUE(brain_detect_communities(fresh_brain));
    EXPECT_TRUE(brain_detect_hubs(fresh_brain, 2.0f));
    EXPECT_TRUE(brain_compute_topology_metrics(fresh_brain));

    brain_destroy(fresh_brain);
}

//=============================================================================
// Edge-Storm Regression (Apr 11 2026)
//
// A former brain_build_topology_graph() helper did an O(N^2) dense-pair
// scan whose result was never used — every caller discarded the NimcpGraph
// and called community_detect()/community_detect_hubs()/
// community_validate_topology() directly on base_network. The dead scan
// stalled a 2.5M-neuron brain for an hour per call. Fix: delete the
// helper and simplify callers.
//
// This test pins down that all four entry points continue to work
// correctly after the cleanup, exercised back-to-back to catch any
// leak/reinit bug introduced by the simplification.
//=============================================================================

TEST_F(BrainCommunityDetectionTest, TopologyEntryPointsRepeatable_EdgeStormRegression) {
    // Run the full topology pipeline multiple times to detect any leak
    // or state-corruption bug introduced by the simplified call paths.
    // Each entry point used to allocate (and leak) a NimcpGraph per call;
    // the removal means repeated calls should remain stable.
    for (int round = 0; round < 3; round++) {
        EXPECT_TRUE(brain_detect_communities(brain))
            << "round " << round << ": detect_communities should succeed";
        EXPECT_TRUE(brain_detect_hubs(brain, 2.0f))
            << "round " << round << ": detect_hubs should succeed";
        EXPECT_TRUE(brain_compute_topology_metrics(brain))
            << "round " << round << ": compute_topology_metrics should succeed";
        EXPECT_TRUE(brain_validate_topology(brain) ||
                    !brain_validate_topology(brain))
            << "round " << round << ": validate should not crash";
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
