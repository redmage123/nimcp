/**
 * @file test_nimcp_occipital_collective_bridge.cpp
 * @brief Unit tests for Occipital-Collective Cognition Bridge
 *
 * WHAT: Tests for occipital-collective integration bridge
 * WHY: Verify joint attention and visual sharing works correctly
 * HOW: Test lifecycle, connection, attention, features, query
 *
 * TEST COVERAGE:
 * - Configuration API (3 tests)
 * - Lifecycle API (5 tests)
 * - Connection API (4 tests)
 * - Update cycle (3 tests)
 * - Joint attention (8 tests)
 * - Feature sharing (6 tests)
 * - Query API (6 tests)
 * - Statistics (3 tests)
 * - Bio-Async API (4 tests)
 * - Instance State (3 tests)
 * - Boundary Value (6 tests)
 * - Max Capacity (2 tests)
 *
 * TOTAL: 53 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_collective_bridge.h"

class OccipitalCollectiveBridgeTest : public ::testing::Test {
protected:
    occipital_collective_bridge_t* bridge;
    occipital_collective_config_t config;

    void SetUp() override {
        occipital_collective_default_config(&config);
        config.enable_bio_async = false;
        bridge = occipital_collective_create(&config, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            occipital_collective_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration API (3 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, DefaultConfigHasReasonableValues) {
    occipital_collective_config_t cfg;
    occipital_collective_default_config(&cfg);

    EXPECT_EQ(cfg.attention_mode, JOINT_ATTENTION_COORDINATE);
    EXPECT_EQ(cfg.sharing_strategy, VISUAL_SHARE_FEATURES);
    EXPECT_TRUE(cfg.share_v1_features);
    EXPECT_TRUE(cfg.share_v2_features);
    EXPECT_TRUE(cfg.share_v4_features);
    EXPECT_TRUE(cfg.share_v5_features);
    EXPECT_GT(cfg.attention_threshold, 0.0f);
    EXPECT_LT(cfg.attention_threshold, 1.0f);
    EXPECT_GT(cfg.update_interval_ms, 0u);
}

TEST_F(OccipitalCollectiveBridgeTest, DefaultConfigWithNullDoesNotCrash) {
    occipital_collective_default_config(nullptr);
    // Should not crash
}

TEST_F(OccipitalCollectiveBridgeTest, CreateWithCustomConfig) {
    occipital_collective_config_t custom_config;
    occipital_collective_default_config(&custom_config);
    custom_config.attention_mode = JOINT_ATTENTION_LEAD;
    custom_config.enable_bio_async = false;

    occipital_collective_bridge_t* custom_bridge = occipital_collective_create(
        &custom_config, nullptr, nullptr);
    ASSERT_NE(custom_bridge, nullptr);
    occipital_collective_destroy(custom_bridge);
}

//=============================================================================
// Lifecycle API (5 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, CreateWithNullConfigUsesDefaults) {
    occipital_collective_bridge_t* b = occipital_collective_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    occipital_collective_destroy(b);
}

TEST_F(OccipitalCollectiveBridgeTest, DestroyNullDoesNotCrash) {
    occipital_collective_destroy(nullptr);
    // Should not crash
}

TEST_F(OccipitalCollectiveBridgeTest, ResetSucceeds) {
    EXPECT_EQ(occipital_collective_reset(bridge), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, ResetNullFails) {
    EXPECT_EQ(occipital_collective_reset(nullptr), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 5; ++i) {
        occipital_collective_bridge_t* b = occipital_collective_create(&config, nullptr, nullptr);
        ASSERT_NE(b, nullptr);
        occipital_collective_destroy(b);
    }
}

//=============================================================================
// Connection API (4 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, ConnectOccipitalWithNull) {
    EXPECT_EQ(occipital_collective_connect_occipital(bridge, nullptr), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, ConnectOccipitalNullBridgeFails) {
    EXPECT_EQ(occipital_collective_connect_occipital(nullptr, nullptr), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, ConnectCollectiveWithNull) {
    EXPECT_EQ(occipital_collective_connect_collective(bridge, nullptr), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, ConnectCollectiveNullBridgeFails) {
    EXPECT_EQ(occipital_collective_connect_collective(nullptr, nullptr), -1);
}

//=============================================================================
// Update Cycle (3 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, UpdateSucceeds) {
    EXPECT_EQ(occipital_collective_update(bridge, 50), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, UpdateNullFails) {
    EXPECT_EQ(occipital_collective_update(nullptr, 50), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, UpdateMultipleTimesSucceeds) {
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(occipital_collective_update(bridge, 20), 0);
    }
}

//=============================================================================
// Joint Attention (8 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, InitiateAttentionSucceeds) {
    uint32_t target_id = 0;
    EXPECT_EQ(occipital_collective_initiate_attention(bridge, 0.5f, 0.5f, 0.8f, &target_id), 0);
    EXPECT_NE(target_id, 0u);
}

TEST_F(OccipitalCollectiveBridgeTest, InitiateAttentionNullBridgeFails) {
    uint32_t target_id = 0;
    EXPECT_EQ(occipital_collective_initiate_attention(nullptr, 0.5f, 0.5f, 0.8f, &target_id), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, InitiateAttentionNullOutputFails) {
    EXPECT_EQ(occipital_collective_initiate_attention(bridge, 0.5f, 0.5f, 0.8f, nullptr), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, FollowAttentionSucceeds) {
    uint32_t target_id = 0;
    EXPECT_EQ(occipital_collective_initiate_attention(bridge, 0.5f, 0.5f, 0.8f, &target_id), 0);
    EXPECT_EQ(occipital_collective_follow_attention(bridge, target_id), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, FollowAttentionInvalidTargetFails) {
    EXPECT_EQ(occipital_collective_follow_attention(bridge, 99999), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, ReleaseAttentionSucceeds) {
    uint32_t target_id = 0;
    EXPECT_EQ(occipital_collective_initiate_attention(bridge, 0.5f, 0.5f, 0.8f, &target_id), 0);
    EXPECT_EQ(occipital_collective_release_attention(bridge, target_id), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, GetTargetsSucceeds) {
    uint32_t target_id = 0;
    EXPECT_EQ(occipital_collective_initiate_attention(bridge, 0.5f, 0.5f, 0.8f, &target_id), 0);

    joint_attention_target_t targets[8];
    uint32_t count = 0;
    EXPECT_EQ(occipital_collective_get_targets(bridge, targets, 8, &count), 0);
    EXPECT_GE(count, 1u);
    EXPECT_EQ(targets[0].target_id, target_id);
}

TEST_F(OccipitalCollectiveBridgeTest, GetTargetsNullFails) {
    joint_attention_target_t targets[8];
    uint32_t count = 0;
    EXPECT_EQ(occipital_collective_get_targets(nullptr, targets, 8, &count), -1);
    EXPECT_EQ(occipital_collective_get_targets(bridge, nullptr, 8, &count), -1);
    EXPECT_EQ(occipital_collective_get_targets(bridge, targets, 8, nullptr), -1);
}

//=============================================================================
// Feature Sharing (6 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, ShareFeatureSucceeds) {
    shared_visual_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    feature.source_area = VISUAL_AREA_V1;
    feature.confidence = 0.9f;
    feature.x = 0.3f;
    feature.y = 0.7f;

    EXPECT_EQ(occipital_collective_share_feature(bridge, &feature), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, ShareFeatureNullFails) {
    shared_visual_feature_t feature;
    EXPECT_EQ(occipital_collective_share_feature(nullptr, &feature), -1);
    EXPECT_EQ(occipital_collective_share_feature(bridge, nullptr), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, GetFeaturesSucceeds) {
    shared_visual_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    feature.source_area = VISUAL_AREA_V1;
    feature.confidence = 0.9f;
    EXPECT_EQ(occipital_collective_share_feature(bridge, &feature), 0);

    shared_visual_feature_t features[64];
    uint32_t count = 0;
    EXPECT_EQ(occipital_collective_get_features(bridge, -1, features, 64, &count), 0);
    EXPECT_GE(count, 1u);
}

TEST_F(OccipitalCollectiveBridgeTest, GetFeaturesFilteredByArea) {
    shared_visual_feature_t f1, f2;
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    f1.source_area = VISUAL_AREA_V1;
    f2.source_area = VISUAL_AREA_V4;
    EXPECT_EQ(occipital_collective_share_feature(bridge, &f1), 0);
    EXPECT_EQ(occipital_collective_share_feature(bridge, &f2), 0);

    shared_visual_feature_t features[64];
    uint32_t count = 0;
    EXPECT_EQ(occipital_collective_get_features(bridge, VISUAL_AREA_V1, features, 64, &count), 0);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(features[0].source_area, VISUAL_AREA_V1);
}

TEST_F(OccipitalCollectiveBridgeTest, MergeFeaturesSucceeds) {
    shared_visual_feature_t f1, f2;
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    f1.source_area = VISUAL_AREA_V1;
    f1.x = 0.5f;
    f1.y = 0.5f;
    f1.confidence = 0.8f;
    f2.source_area = VISUAL_AREA_V1;
    f2.x = 0.51f;  // Very close to f1
    f2.y = 0.51f;
    f2.confidence = 0.7f;

    EXPECT_EQ(occipital_collective_share_feature(bridge, &f1), 0);
    EXPECT_EQ(occipital_collective_share_feature(bridge, &f2), 0);

    int merged = occipital_collective_merge_features(bridge, 0.1f);
    EXPECT_GE(merged, 0);
}

TEST_F(OccipitalCollectiveBridgeTest, MergeFeaturesNullFails) {
    EXPECT_EQ(occipital_collective_merge_features(nullptr, 0.1f), -1);
}

//=============================================================================
// Query API (6 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, GetSummarySucceeds) {
    // Update to register local instance
    occipital_collective_update(bridge, 50);

    collective_visual_summary_t summary;
    EXPECT_EQ(occipital_collective_get_summary(bridge, &summary), 0);
    EXPECT_GE(summary.total_instances, 1u);
}

TEST_F(OccipitalCollectiveBridgeTest, GetSummaryNullFails) {
    collective_visual_summary_t summary;
    EXPECT_EQ(occipital_collective_get_summary(nullptr, &summary), -1);
    EXPECT_EQ(occipital_collective_get_summary(bridge, nullptr), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, GetLocalIdReturnsNonZero) {
    uint32_t id = occipital_collective_get_local_id(bridge);
    EXPECT_NE(id, 0u);
}

TEST_F(OccipitalCollectiveBridgeTest, GetLocalIdNullReturnsZero) {
    EXPECT_EQ(occipital_collective_get_local_id(nullptr), 0u);
}

TEST_F(OccipitalCollectiveBridgeTest, GetCoherenceReturnsValid) {
    float coherence = occipital_collective_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(OccipitalCollectiveBridgeTest, IsAttentionLeaderWorks) {
    // Should return true or false without crashing
    bool result = occipital_collective_is_attention_leader(bridge);
    (void)result;  // Value depends on state
}

//=============================================================================
// Statistics (3 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, GetStatsSucceeds) {
    occipital_collective_stats_t stats;
    EXPECT_EQ(occipital_collective_get_stats(bridge, &stats), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, GetStatsNullFails) {
    occipital_collective_stats_t stats;
    EXPECT_EQ(occipital_collective_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(occipital_collective_get_stats(bridge, nullptr), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, ResetStatsSucceeds) {
    uint32_t target_id = 0;
    occipital_collective_initiate_attention(bridge, 0.5f, 0.5f, 0.8f, &target_id);

    occipital_collective_stats_t stats;
    occipital_collective_get_stats(bridge, &stats);
    EXPECT_GE(stats.joint_targets_created, 1u);

    occipital_collective_reset_stats(bridge);
    occipital_collective_get_stats(bridge, &stats);
    EXPECT_EQ(stats.joint_targets_created, 0u);
}

//=============================================================================
// Bio-Async API Tests (4 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, ConnectBioAsyncSucceeds) {
    EXPECT_EQ(occipital_collective_connect_bio_async(bridge), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, ConnectBioAsyncNullFails) {
    EXPECT_EQ(occipital_collective_connect_bio_async(nullptr), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, DisconnectBioAsyncSucceeds) {
    occipital_collective_connect_bio_async(bridge);
    EXPECT_EQ(occipital_collective_disconnect_bio_async(bridge), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, DisconnectBioAsyncNullFails) {
    EXPECT_EQ(occipital_collective_disconnect_bio_async(nullptr), -1);
}

//=============================================================================
// Instance State Tests (3 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, GetInstanceStateSucceeds) {
    occipital_collective_update(bridge, 50);
    uint32_t local_id = occipital_collective_get_local_id(bridge);
    collective_visual_state_t state;
    EXPECT_EQ(occipital_collective_get_instance_state(bridge, local_id, &state), 0);
    EXPECT_EQ(state.instance_id, local_id);
}

TEST_F(OccipitalCollectiveBridgeTest, GetInstanceStateNotFound) {
    collective_visual_state_t state;
    EXPECT_EQ(occipital_collective_get_instance_state(bridge, 99999, &state), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, GetInstanceStateNullFails) {
    collective_visual_state_t state;
    EXPECT_EQ(occipital_collective_get_instance_state(nullptr, 1, &state), -1);
    EXPECT_EQ(occipital_collective_get_instance_state(bridge, 1, nullptr), -1);
}

//=============================================================================
// Boundary Value Tests (6 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, InitiateAttentionAtOrigin) {
    uint32_t target_id = 0;
    EXPECT_EQ(occipital_collective_initiate_attention(bridge, 0.0f, 0.0f, 0.5f, &target_id), 0);
    EXPECT_NE(target_id, 0u);
}

TEST_F(OccipitalCollectiveBridgeTest, InitiateAttentionAtMax) {
    uint32_t target_id = 0;
    EXPECT_EQ(occipital_collective_initiate_attention(bridge, 1.0f, 1.0f, 1.0f, &target_id), 0);
    EXPECT_NE(target_id, 0u);
}

TEST_F(OccipitalCollectiveBridgeTest, ShareFeatureMinConfidence) {
    shared_visual_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    feature.source_area = VISUAL_AREA_V1;
    feature.confidence = 0.0f;
    EXPECT_EQ(occipital_collective_share_feature(bridge, &feature), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, ShareFeatureMaxConfidence) {
    shared_visual_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    feature.source_area = VISUAL_AREA_V1;
    feature.confidence = 1.0f;
    EXPECT_EQ(occipital_collective_share_feature(bridge, &feature), 0);
}

TEST_F(OccipitalCollectiveBridgeTest, MergeFeaturesZeroTolerance) {
    int merged = occipital_collective_merge_features(bridge, 0.0f);
    EXPECT_GE(merged, 0);
}

TEST_F(OccipitalCollectiveBridgeTest, MergeFeaturesLargeTolerance) {
    // Add two features at same location
    shared_visual_feature_t f1, f2;
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    f1.source_area = VISUAL_AREA_V1;
    f1.x = 0.5f;
    f1.y = 0.5f;
    f2.source_area = VISUAL_AREA_V1;
    f2.x = 0.5f;
    f2.y = 0.5f;
    occipital_collective_share_feature(bridge, &f1);
    occipital_collective_share_feature(bridge, &f2);

    int merged = occipital_collective_merge_features(bridge, 1.0f);
    EXPECT_GE(merged, 1);  // Should merge the two identical features
}

//=============================================================================
// Max Capacity Tests (2 tests)
//=============================================================================

TEST_F(OccipitalCollectiveBridgeTest, MaxTargetsReached) {
    // Fill up targets
    for (int i = 0; i < OCCIPITAL_COLLECTIVE_MAX_TARGETS; ++i) {
        uint32_t id;
        EXPECT_EQ(occipital_collective_initiate_attention(bridge, 0.5f, 0.5f, 0.5f, &id), 0);
    }
    // Next should fail
    uint32_t id;
    EXPECT_EQ(occipital_collective_initiate_attention(bridge, 0.5f, 0.5f, 0.5f, &id), -1);
}

TEST_F(OccipitalCollectiveBridgeTest, FeatureEvictionOnFull) {
    // Fill up features
    for (int i = 0; i < OCCIPITAL_COLLECTIVE_MAX_FEATURES + 5; ++i) {
        shared_visual_feature_t feature;
        memset(&feature, 0, sizeof(feature));
        feature.source_area = VISUAL_AREA_V1;
        feature.confidence = 0.5f;
        EXPECT_EQ(occipital_collective_share_feature(bridge, &feature), 0);
    }
    // Should still work - old features evicted
    shared_visual_feature_t features[64];
    uint32_t count = 0;
    EXPECT_EQ(occipital_collective_get_features(bridge, -1, features, 64, &count), 0);
    EXPECT_EQ(count, (uint32_t)OCCIPITAL_COLLECTIVE_MAX_FEATURES);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
