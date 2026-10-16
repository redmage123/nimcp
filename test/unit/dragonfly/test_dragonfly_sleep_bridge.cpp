/**
 * @file test_dragonfly_sleep_bridge.cpp
 * @brief Unit tests for sleep-dragonfly integration bridge
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_sleep_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SleepBridgeTest : public ::testing::Test {
protected:
    dragonfly_sleep_bridge_t bridge = nullptr;

    void SetUp() override {
        bridge = dragonfly_sleep_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_sleep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(SleepBridgeTest, DefaultConfig) {
    dragonfly_sleep_config_t config = dragonfly_sleep_default_config();
    EXPECT_TRUE(config.enable_memory_consolidation);
    EXPECT_GT(config.min_experiences_to_consolidate, 0u);
    EXPECT_GT(config.dawn_activity_boost, 0.0f);
}

TEST_F(SleepBridgeTest, ValidateConfig) {
    dragonfly_sleep_config_t config = dragonfly_sleep_default_config();
    EXPECT_TRUE(dragonfly_sleep_validate_config(&config));

    config.consolidation_threshold = -0.1f;  // Invalid
    EXPECT_FALSE(dragonfly_sleep_validate_config(&config));

    config = dragonfly_sleep_default_config();
    config.prey_wake_threshold = 1.5f;  // Invalid
    EXPECT_FALSE(dragonfly_sleep_validate_config(&config));

    EXPECT_FALSE(dragonfly_sleep_validate_config(nullptr));
}

TEST_F(SleepBridgeTest, CreateWithCustomConfig) {
    dragonfly_sleep_config_t config = dragonfly_sleep_default_config();
    config.min_experiences_to_consolidate = 10;
    config.dawn_activity_boost = 1.5f;

    dragonfly_sleep_bridge_t custom = dragonfly_sleep_bridge_create(&config);
    ASSERT_NE(custom, nullptr);
    dragonfly_sleep_bridge_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SleepBridgeTest, CreateAndDestroy) {
    dragonfly_sleep_bridge_t b = dragonfly_sleep_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    dragonfly_sleep_bridge_destroy(b);
}

TEST_F(SleepBridgeTest, DestroyNull) {
    dragonfly_sleep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(SleepBridgeTest, Connect) {
    EXPECT_EQ(dragonfly_sleep_bridge_connect(bridge, nullptr, nullptr), 0);
}

TEST_F(SleepBridgeTest, Disconnect) {
    dragonfly_sleep_bridge_connect(bridge, nullptr, nullptr);
    EXPECT_EQ(dragonfly_sleep_bridge_disconnect(bridge), 0);
}

//=============================================================================
// Experience Recording Tests
//=============================================================================

TEST_F(SleepBridgeTest, RecordSuccess) {
    EXPECT_EQ(dragonfly_sleep_record_success(bridge, 1, 0.05f, INTERCEPT_PURSUIT), 0);
}

TEST_F(SleepBridgeTest, RecordFailure) {
    EXPECT_EQ(dragonfly_sleep_record_failure(bridge, 2, "target_escaped", INTERCEPT_LEAD), 0);
}

TEST_F(SleepBridgeTest, RecordMultipleExperiences) {
    for (int i = 0; i < 10; i++) {
        if (i % 3 == 0) {
            dragonfly_sleep_record_success(bridge, i, 0.05f, INTERCEPT_PN);
        } else {
            dragonfly_sleep_record_failure(bridge, i, "missed", INTERCEPT_PURSUIT);
        }
    }

    dragonfly_sleep_state_t state;
    dragonfly_sleep_get_state(bridge, &state);
    EXPECT_EQ(state.pending_experiences, 10u);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(SleepBridgeTest, BasicUpdate) {
    EXPECT_EQ(dragonfly_sleep_bridge_update(bridge, 0.016f), 0);
}

TEST_F(SleepBridgeTest, CircadianModulation) {
    dragonfly_sleep_bridge_update(bridge, 0.1f);

    float activity = dragonfly_sleep_get_activity(bridge);
    EXPECT_GE(activity, 0.0f);
    EXPECT_LE(activity, 1.5f);
}

TEST_F(SleepBridgeTest, HuntingAllowedCheck) {
    dragonfly_sleep_bridge_update(bridge, 0.1f);
    bool allowed = dragonfly_sleep_hunting_allowed(bridge);
    // Depends on time of day, so just check it returns a valid value
    (void)allowed;
}

//=============================================================================
// Memory Consolidation Tests
//=============================================================================

TEST_F(SleepBridgeTest, ConsolidateWithEnoughExperiences) {
    // Record enough experiences
    for (uint32_t i = 0; i < 10; i++) {
        dragonfly_sleep_record_success(bridge, i, 0.05f, INTERCEPT_PURSUIT);
    }

    EXPECT_EQ(dragonfly_sleep_consolidate(bridge), 0);

    dragonfly_sleep_state_t state;
    dragonfly_sleep_get_state(bridge, &state);
    EXPECT_EQ(state.pending_experiences, 0u);  // Should be cleared
}

TEST_F(SleepBridgeTest, ConsolidateWithTooFewExperiences) {
    // Record only 2 experiences (default minimum is 5)
    dragonfly_sleep_record_success(bridge, 1, 0.05f, INTERCEPT_PURSUIT);
    dragonfly_sleep_record_success(bridge, 2, 0.05f, INTERCEPT_LEAD);

    EXPECT_EQ(dragonfly_sleep_consolidate(bridge), 0);

    dragonfly_sleep_state_t state;
    dragonfly_sleep_get_state(bridge, &state);
    EXPECT_EQ(state.pending_experiences, 2u);  // Not cleared
}

TEST_F(SleepBridgeTest, GetConsolidatedMemory) {
    for (uint32_t i = 0; i < 10; i++) {
        dragonfly_sleep_record_success(bridge, i, 0.05f, INTERCEPT_PURSUIT);
    }
    dragonfly_sleep_consolidate(bridge);

    consolidated_memory_t memory;
    EXPECT_EQ(dragonfly_sleep_get_memory(bridge, &memory), 0);
    EXPECT_GT(memory.experiences_processed, 0u);
}

//=============================================================================
// Strategy Recommendation Tests
//=============================================================================

TEST_F(SleepBridgeTest, RecommendStrategy) {
    // Record some experiences
    for (int i = 0; i < 10; i++) {
        dragonfly_sleep_record_success(bridge, i, 0.05f, INTERCEPT_PURSUIT);
    }
    dragonfly_sleep_consolidate(bridge);

    intercept_strategy_t strategy;
    float confidence;
    EXPECT_EQ(dragonfly_sleep_recommend_strategy(bridge, 0, 12.0f, &strategy, &confidence), 0);

    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(SleepBridgeTest, GetState) {
    dragonfly_sleep_state_t state;
    EXPECT_EQ(dragonfly_sleep_get_state(bridge, &state), 0);
    EXPECT_GE(state.activity_level, 0.0f);
}

TEST_F(SleepBridgeTest, GetStats) {
    dragonfly_sleep_stats_t stats;
    EXPECT_EQ(dragonfly_sleep_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.experiences_recorded, 0u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(SleepBridgeTest, NullPointerHandling) {
    dragonfly_sleep_state_t state;

    EXPECT_EQ(dragonfly_sleep_bridge_update(nullptr, 0.016f), -1);
    EXPECT_EQ(dragonfly_sleep_record_success(nullptr, 1, 0.05f, INTERCEPT_PURSUIT), -1);
    EXPECT_EQ(dragonfly_sleep_consolidate(nullptr), -1);
    EXPECT_EQ(dragonfly_sleep_get_state(nullptr, &state), -1);
    EXPECT_EQ(dragonfly_sleep_get_state(bridge, nullptr), -1);
    EXPECT_FALSE(dragonfly_sleep_hunting_allowed(nullptr));
}

TEST_F(SleepBridgeTest, InvalidDeltaTime) {
    EXPECT_EQ(dragonfly_sleep_bridge_update(bridge, -1.0f), -1);
    EXPECT_EQ(dragonfly_sleep_bridge_update(bridge, 0.0f), -1);
}
