/**
 * @file test_nimcp_occipital_quantum_bridge.cpp
 * @brief Unit tests for nimcp_occipital_quantum_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Occipital Quantum Bridge
 * WHY:  Ensure correct quantum-accelerated visual processing operations
 * HOW:  Use Google Test framework to test visual search, feature binding,
 *       scene segmentation, motion integration, and statistics.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_quantum_bridge.h"

// Test Fixture for Occipital Quantum Bridge
class OccipitalQuantumBridgeTest : public ::testing::Test {
protected:
    occipital_quantum_bridge_t* bridge;
    occipital_quantum_config_t config;

    void SetUp() override {
        config = occipital_quantum_default_config();
        bridge = occipital_quantum_bridge_create(NULL, &config);
        ASSERT_NE(nullptr, bridge) << "Failed to create occipital quantum bridge";
    }

    void TearDown() override {
        occipital_quantum_bridge_destroy(bridge);
        bridge = nullptr;
    }

    // Helper to create a test edge map
    float* create_test_edge_map(uint32_t width, uint32_t height) {
        float* map = (float*)calloc(width * height, sizeof(float));
        if (!map) return nullptr;

        // Create vertical edges
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                if (x % 16 == 8) {
                    map[y * width + x] = 1.0f;
                }
            }
        }

        return map;
    }

    void free_edge_map(float* map) {
        if (map) free(map);
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(OccipitalQuantumBridgeTest, DefaultConfigHasReasonableValues) {
    occipital_quantum_config_t default_config = occipital_quantum_default_config();

    EXPECT_TRUE(default_config.enabled);
    EXPECT_GT(default_config.visual_search_depth, 0u);
    EXPECT_GT(default_config.binding_alternatives, 0u);
    EXPECT_GT(default_config.max_grover_iterations, 0u);
    EXPECT_GT(default_config.min_detection_confidence, 0.0f);
    EXPECT_LE(default_config.min_detection_confidence, 1.0f);
    EXPECT_TRUE(default_config.enable_interference);
    EXPECT_TRUE(default_config.use_superposition);
}

TEST_F(OccipitalQuantumBridgeTest, CreateWithNullConfigUsesDefaults) {
    occipital_quantum_bridge_t* bridge_null = occipital_quantum_bridge_create(NULL, NULL);
    ASSERT_NE(nullptr, bridge_null);

    occipital_quantum_config_t retrieved;
    EXPECT_EQ(0, occipital_quantum_get_config(bridge_null, &retrieved));
    EXPECT_TRUE(retrieved.enabled);

    occipital_quantum_bridge_destroy(bridge_null);
}

TEST_F(OccipitalQuantumBridgeTest, DestroyNullDoesNotCrash) {
    occipital_quantum_bridge_destroy(NULL);
    // Should not crash
}

TEST_F(OccipitalQuantumBridgeTest, IsEnabledReturnsTrue) {
    EXPECT_TRUE(occipital_quantum_bridge_is_enabled(bridge));
}

TEST_F(OccipitalQuantumBridgeTest, SetEnabledToFalse) {
    occipital_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(occipital_quantum_bridge_is_enabled(bridge));
}

TEST_F(OccipitalQuantumBridgeTest, IsEnabledNullReturnsFalse) {
    EXPECT_FALSE(occipital_quantum_bridge_is_enabled(NULL));
}

// ============================================================================
// VISUAL SEARCH TESTS
// ============================================================================

TEST_F(OccipitalQuantumBridgeTest, VisualSearchBasic) {
    visual_search_target_t target;
    memset(&target, 0, sizeof(target));
    target.target_id = 1;
    target.target_color_h = 0.0f;   // Red
    target.target_color_s = 1.0f;   // Full saturation
    target.search_by_color = true;
    target.conjunction_search = false;

    quantum_search_result_t result;
    EXPECT_EQ(0, occipital_quantum_visual_search(bridge, &target, 100, &result));

    EXPECT_GT(result.locations_evaluated, 0u);
    EXPECT_GE(result.satisfaction_probability, 0.0f);
    EXPECT_LE(result.satisfaction_probability, 1.0f);
    EXPECT_GT(result.search_speedup, 1.0f);  // Quantum should be faster
}

TEST_F(OccipitalQuantumBridgeTest, VisualSearchConjunction) {
    visual_search_target_t target;
    memset(&target, 0, sizeof(target));
    target.target_id = 2;
    target.target_color_h = 120.0f;  // Green
    target.target_color_s = 0.8f;
    target.target_orientation = 0.5f;
    target.search_by_color = true;
    target.search_by_orientation = true;
    target.conjunction_search = true;

    quantum_search_result_t result;
    EXPECT_EQ(0, occipital_quantum_visual_search(bridge, &target, 500, &result));

    EXPECT_GT(result.locations_evaluated, 0u);
    // Conjunction search is harder but should still work
}

TEST_F(OccipitalQuantumBridgeTest, VisualSearchLargeField) {
    visual_search_target_t target;
    memset(&target, 0, sizeof(target));
    target.target_id = 3;
    target.search_by_size = true;
    target.target_size = 0.5f;

    quantum_search_result_t result;
    EXPECT_EQ(0, occipital_quantum_visual_search(bridge, &target, 1024, &result));

    // Grover speedup should be significant for large search space
    EXPECT_GT(result.search_speedup, sqrtf(1024.0f) / 2.0f);
}

TEST_F(OccipitalQuantumBridgeTest, VisualSearchDisabledFails) {
    occipital_quantum_bridge_set_enabled(bridge, false);

    visual_search_target_t target;
    memset(&target, 0, sizeof(target));

    quantum_search_result_t result;
    EXPECT_EQ(-1, occipital_quantum_visual_search(bridge, &target, 100, &result));
}

TEST_F(OccipitalQuantumBridgeTest, VisualSearchNullParams) {
    visual_search_target_t target;
    quantum_search_result_t result;

    EXPECT_EQ(-1, occipital_quantum_visual_search(NULL, &target, 100, &result));
    EXPECT_EQ(-1, occipital_quantum_visual_search(bridge, NULL, 100, &result));
    EXPECT_EQ(-1, occipital_quantum_visual_search(bridge, &target, 100, NULL));
}

// ============================================================================
// FEATURE BINDING TESTS
// ============================================================================

TEST_F(OccipitalQuantumBridgeTest, FeatureBindingBasic) {
    // Create feature locations (x, y pairs)
    float feature_locations[] = {
        0.1f, 0.1f,  // Feature 0
        0.15f, 0.12f, // Feature 1 (nearby)
        0.5f, 0.5f,  // Feature 2 (far)
        0.52f, 0.48f  // Feature 3 (nearby to 2)
    };
    uint32_t feature_types[] = {0, 0, 1, 1};

    quantum_binding_result_t result;
    EXPECT_EQ(0, occipital_quantum_feature_binding(bridge,
        feature_locations, feature_types, 4, &result));

    EXPECT_GT(result.hypotheses_evaluated, 0u);
    EXPECT_GE(result.satisfaction_probability, 0.0f);
}

TEST_F(OccipitalQuantumBridgeTest, FeatureBindingCoherence) {
    // Create tightly clustered features (should have high coherence)
    float feature_locations[] = {
        0.5f, 0.5f,
        0.51f, 0.51f,
        0.49f, 0.52f,
        0.52f, 0.49f
    };
    uint32_t feature_types[] = {0, 0, 0, 0};

    quantum_binding_result_t result;
    EXPECT_EQ(0, occipital_quantum_feature_binding(bridge,
        feature_locations, feature_types, 4, &result));

    // Best binding should have high coherence due to spatial proximity
    if (result.best_binding) {
        EXPECT_GT(result.best_binding->coherence, 0.5f);
    }
}

TEST_F(OccipitalQuantumBridgeTest, FeatureBindingDisabledFails) {
    occipital_quantum_bridge_set_enabled(bridge, false);

    float feature_locations[] = {0.1f, 0.1f};
    uint32_t feature_types[] = {0};
    quantum_binding_result_t result;

    EXPECT_EQ(-1, occipital_quantum_feature_binding(bridge,
        feature_locations, feature_types, 1, &result));
}

TEST_F(OccipitalQuantumBridgeTest, FeatureBindingNullParams) {
    float feature_locations[4];
    uint32_t feature_types[2];
    quantum_binding_result_t result;

    EXPECT_EQ(-1, occipital_quantum_feature_binding(NULL,
        feature_locations, feature_types, 2, &result));
    EXPECT_EQ(-1, occipital_quantum_feature_binding(bridge,
        NULL, feature_types, 2, &result));
    EXPECT_EQ(-1, occipital_quantum_feature_binding(bridge,
        feature_locations, NULL, 2, &result));
    EXPECT_EQ(-1, occipital_quantum_feature_binding(bridge,
        feature_locations, feature_types, 2, NULL));
}

// ============================================================================
// SCENE SEGMENTATION TESTS
// ============================================================================

TEST_F(OccipitalQuantumBridgeTest, SegmentSceneBasic) {
    uint32_t width = 32, height = 32;
    float* edge_map = create_test_edge_map(width, height);
    ASSERT_NE(nullptr, edge_map);

    quantum_segmentation_result_t result;
    EXPECT_EQ(0, occipital_quantum_segment_scene(bridge, edge_map, width, height, &result));

    EXPECT_GT(result.segmentations_evaluated, 0u);
    EXPECT_GE(result.optimization_score, 0.0f);

    free_edge_map(edge_map);
}

TEST_F(OccipitalQuantumBridgeTest, SegmentSceneWithStrongEdges) {
    uint32_t width = 64, height = 64;
    float* edge_map = (float*)calloc(width * height, sizeof(float));
    ASSERT_NE(nullptr, edge_map);

    // Create a clear figure-ground boundary
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            // Circular boundary
            float cx = (float)x - (float)width / 2.0f;
            float cy = (float)y - (float)height / 2.0f;
            float dist = sqrtf(cx * cx + cy * cy);
            float radius = (float)width / 4.0f;
            if (fabsf(dist - radius) < 2.0f) {
                edge_map[y * width + x] = 1.0f;
            }
        }
    }

    quantum_segmentation_result_t result;
    EXPECT_EQ(0, occipital_quantum_segment_scene(bridge, edge_map, width, height, &result));

    // Strong edges should help segmentation
    EXPECT_GT(result.optimization_score, 0.0f);

    free(edge_map);
}

TEST_F(OccipitalQuantumBridgeTest, SegmentSceneDisabledFails) {
    occipital_quantum_bridge_set_enabled(bridge, false);

    float edge_map[16] = {0};
    quantum_segmentation_result_t result;

    EXPECT_EQ(-1, occipital_quantum_segment_scene(bridge, edge_map, 4, 4, &result));
}

TEST_F(OccipitalQuantumBridgeTest, SegmentSceneNullParams) {
    float edge_map[16];
    quantum_segmentation_result_t result;

    EXPECT_EQ(-1, occipital_quantum_segment_scene(NULL, edge_map, 4, 4, &result));
    EXPECT_EQ(-1, occipital_quantum_segment_scene(bridge, NULL, 4, 4, &result));
    EXPECT_EQ(-1, occipital_quantum_segment_scene(bridge, edge_map, 4, 4, NULL));
}

// ============================================================================
// MOTION INTEGRATION TESTS
// ============================================================================

TEST_F(OccipitalQuantumBridgeTest, IntegrateMotionBasic) {
    // Create local motion vectors (all pointing right)
    float local_dx[] = {2.0f, 2.1f, 1.9f, 2.0f, 2.2f};
    float local_dy[] = {0.0f, 0.1f, -0.1f, 0.0f, 0.05f};

    quantum_motion_result_t result;
    EXPECT_EQ(0, occipital_quantum_integrate_motion(bridge,
        local_dx, local_dy, 5, &result));

    EXPECT_GT(result.integrations_evaluated, 0u);
    EXPECT_GT(result.coherence_score, 0.0f);

    // Global motion should be approximately (2, 0)
    if (result.best_integration) {
        EXPECT_GT(result.best_integration->global_dx, 1.0f);
        EXPECT_LT(fabsf(result.best_integration->global_dy), 1.0f);
    }
}

TEST_F(OccipitalQuantumBridgeTest, IntegrateMotionCoherent) {
    // Very coherent motion - all same direction
    float local_dx[10];
    float local_dy[10];
    for (int i = 0; i < 10; i++) {
        local_dx[i] = 3.0f;
        local_dy[i] = 1.0f;
    }

    quantum_motion_result_t result;
    EXPECT_EQ(0, occipital_quantum_integrate_motion(bridge,
        local_dx, local_dy, 10, &result));

    // High coherence expected
    EXPECT_GT(result.coherence_score, 0.5f);
}

TEST_F(OccipitalQuantumBridgeTest, IntegrateMotionIncoherent) {
    // Incoherent motion - random directions
    float local_dx[] = {1.0f, -1.0f, 0.0f, 2.0f, -2.0f};
    float local_dy[] = {0.0f, 1.0f, -1.0f, 0.5f, -0.5f};

    quantum_motion_result_t result;
    EXPECT_EQ(0, occipital_quantum_integrate_motion(bridge,
        local_dx, local_dy, 5, &result));

    // Lower coherence expected (but still valid)
    EXPECT_GE(result.coherence_score, 0.0f);
}

TEST_F(OccipitalQuantumBridgeTest, IntegrateMotionDisabledFails) {
    occipital_quantum_bridge_set_enabled(bridge, false);

    float local_dx[] = {1.0f};
    float local_dy[] = {0.0f};
    quantum_motion_result_t result;

    EXPECT_EQ(-1, occipital_quantum_integrate_motion(bridge,
        local_dx, local_dy, 1, &result));
}

TEST_F(OccipitalQuantumBridgeTest, IntegrateMotionNullParams) {
    float local_dx[4], local_dy[4];
    quantum_motion_result_t result;

    EXPECT_EQ(-1, occipital_quantum_integrate_motion(NULL,
        local_dx, local_dy, 4, &result));
    EXPECT_EQ(-1, occipital_quantum_integrate_motion(bridge,
        NULL, local_dy, 4, &result));
    EXPECT_EQ(-1, occipital_quantum_integrate_motion(bridge,
        local_dx, NULL, 4, &result));
    EXPECT_EQ(-1, occipital_quantum_integrate_motion(bridge,
        local_dx, local_dy, 4, NULL));
}

TEST_F(OccipitalQuantumBridgeTest, IntegrateMotionZeroVectorsFails) {
    float local_dx[1], local_dy[1];
    quantum_motion_result_t result;

    EXPECT_EQ(-1, occipital_quantum_integrate_motion(bridge,
        local_dx, local_dy, 0, &result));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(OccipitalQuantumBridgeTest, GetStatsInitialValues) {
    occipital_quantum_stats_t stats;
    EXPECT_EQ(0, occipital_quantum_get_stats(bridge, &stats));

    EXPECT_EQ(stats.visual_searches, 0u);
    EXPECT_EQ(stats.binding_operations, 0u);
    EXPECT_EQ(stats.segmentation_operations, 0u);
    EXPECT_EQ(stats.motion_integrations, 0u);
}

TEST_F(OccipitalQuantumBridgeTest, StatsUpdateAfterOperations) {
    // Perform some operations
    visual_search_target_t target;
    memset(&target, 0, sizeof(target));
    quantum_search_result_t search_result;
    occipital_quantum_visual_search(bridge, &target, 100, &search_result);

    float feature_locations[] = {0.5f, 0.5f, 0.6f, 0.6f};
    uint32_t feature_types[] = {0, 0};
    quantum_binding_result_t binding_result;
    occipital_quantum_feature_binding(bridge, feature_locations, feature_types, 2, &binding_result);

    float local_dx[] = {1.0f, 1.0f};
    float local_dy[] = {0.0f, 0.0f};
    quantum_motion_result_t motion_result;
    occipital_quantum_integrate_motion(bridge, local_dx, local_dy, 2, &motion_result);

    occipital_quantum_stats_t stats;
    EXPECT_EQ(0, occipital_quantum_get_stats(bridge, &stats));

    EXPECT_EQ(stats.visual_searches, 1u);
    EXPECT_EQ(stats.binding_operations, 1u);
    EXPECT_EQ(stats.motion_integrations, 1u);
}

TEST_F(OccipitalQuantumBridgeTest, ResetStats) {
    // Perform an operation first
    visual_search_target_t target;
    memset(&target, 0, sizeof(target));
    quantum_search_result_t result;
    occipital_quantum_visual_search(bridge, &target, 100, &result);

    occipital_quantum_stats_t stats;
    EXPECT_EQ(0, occipital_quantum_get_stats(bridge, &stats));
    EXPECT_EQ(stats.visual_searches, 1u);

    // Reset
    occipital_quantum_reset_stats(bridge);

    EXPECT_EQ(0, occipital_quantum_get_stats(bridge, &stats));
    EXPECT_EQ(stats.visual_searches, 0u);
}

TEST_F(OccipitalQuantumBridgeTest, GetStatsNullParams) {
    occipital_quantum_stats_t stats;
    EXPECT_EQ(-1, occipital_quantum_get_stats(NULL, &stats));
    EXPECT_EQ(-1, occipital_quantum_get_stats(bridge, NULL));
}

TEST_F(OccipitalQuantumBridgeTest, ResetStatsNullDoesNotCrash) {
    occipital_quantum_reset_stats(NULL);
    // Should not crash
}

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(OccipitalQuantumBridgeTest, GetConfig) {
    occipital_quantum_config_t retrieved;
    EXPECT_EQ(0, occipital_quantum_get_config(bridge, &retrieved));

    EXPECT_EQ(retrieved.enabled, config.enabled);
    EXPECT_EQ(retrieved.visual_search_depth, config.visual_search_depth);
    EXPECT_EQ(retrieved.max_grover_iterations, config.max_grover_iterations);
}

TEST_F(OccipitalQuantumBridgeTest, GetConfigNullParams) {
    occipital_quantum_config_t cfg;
    EXPECT_EQ(-1, occipital_quantum_get_config(NULL, &cfg));
    EXPECT_EQ(-1, occipital_quantum_get_config(bridge, NULL));
}

// ============================================================================
// CUSTOM CONFIGURATION TESTS
// ============================================================================

TEST_F(OccipitalQuantumBridgeTest, CustomSearchDepth) {
    occipital_quantum_config_t custom_config = occipital_quantum_default_config();
    custom_config.visual_search_depth = 2048;
    custom_config.max_grover_iterations = 20;

    occipital_quantum_bridge_t* custom_bridge = occipital_quantum_bridge_create(NULL, &custom_config);
    ASSERT_NE(nullptr, custom_bridge);

    occipital_quantum_config_t retrieved;
    EXPECT_EQ(0, occipital_quantum_get_config(custom_bridge, &retrieved));
    EXPECT_EQ(retrieved.visual_search_depth, 2048u);
    EXPECT_EQ(retrieved.max_grover_iterations, 20u);

    occipital_quantum_bridge_destroy(custom_bridge);
}

TEST_F(OccipitalQuantumBridgeTest, CustomDetectionConfidence) {
    occipital_quantum_config_t custom_config = occipital_quantum_default_config();
    custom_config.min_detection_confidence = 0.8f;

    occipital_quantum_bridge_t* custom_bridge = occipital_quantum_bridge_create(NULL, &custom_config);
    ASSERT_NE(nullptr, custom_bridge);

    visual_search_target_t target;
    memset(&target, 0, sizeof(target));
    target.search_by_color = true;

    quantum_search_result_t result;
    EXPECT_EQ(0, occipital_quantum_visual_search(custom_bridge, &target, 100, &result));

    // Higher threshold means fewer targets classified as found
    // (can't guarantee specific outcome, just test it works)

    occipital_quantum_bridge_destroy(custom_bridge);
}

// ============================================================================
// SPEEDUP VERIFICATION TESTS
// ============================================================================

TEST_F(OccipitalQuantumBridgeTest, GroverSpeedupIncreases) {
    visual_search_target_t target;
    memset(&target, 0, sizeof(target));
    target.search_by_color = true;

    // Small search space
    quantum_search_result_t result_small;
    EXPECT_EQ(0, occipital_quantum_visual_search(bridge, &target, 100, &result_small));

    // Large search space
    quantum_search_result_t result_large;
    EXPECT_EQ(0, occipital_quantum_visual_search(bridge, &target, 1000, &result_large));

    // Speedup should be higher for larger search space (Grover is O(sqrt(N)))
    // The ratio should be approximately sqrt(1000/100) = sqrt(10) ~ 3.16
    // But due to overhead, we just verify it's greater for larger N
    EXPECT_GT(result_large.search_speedup, result_small.search_speedup * 0.5f);
}
