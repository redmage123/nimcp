/**
 * @file test_dragonfly_multi_target.cpp
 * @brief Unit tests for multi-target priority queue module
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_multi_target.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MultiTargetTest : public ::testing::Test {
protected:
    dragonfly_multi_target_t mt = nullptr;

    void SetUp() override {
        mt = dragonfly_multi_target_create(nullptr);
        ASSERT_NE(mt, nullptr);
    }

    void TearDown() override {
        if (mt) {
            dragonfly_multi_target_destroy(mt);
            mt = nullptr;
        }
    }

    dragonfly_detection_t make_detection(uint32_t id, float x, float y, float z,
                                          float dir, float speed, float size) {
        dragonfly_detection_t det = {};
        det.id = id;
        det.position[0] = x;
        det.position[1] = y;
        det.position[2] = z;
        det.motion_direction_rad = dir;
        det.motion_speed = speed;
        det.size = size;
        det.contrast = 0.8f;
        det.timestamp_us = 1000000;
        return det;
    }

    dragonfly_self_state_t make_self_state(float x, float y, float z) {
        dragonfly_self_state_t self = {};
        self.position[0] = x;
        self.position[1] = y;
        self.position[2] = z;
        self.max_speed = 15.0f;
        self.max_accel = 10.0f;
        self.max_turn_rate = 3.0f;
        self.energy_level = 0.8f;
        return self;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(MultiTargetTest, DefaultConfig) {
    multi_target_config_t config = multi_target_default_config();
    EXPECT_GT(config.max_queue_size, 0u);
    EXPECT_GT(config.min_confidence_threshold, 0.0f);
    EXPECT_LE(config.min_confidence_threshold, 1.0f);
}

TEST_F(MultiTargetTest, ValidateConfig) {
    multi_target_config_t config = multi_target_default_config();
    EXPECT_TRUE(multi_target_validate_config(&config));

    config.max_queue_size = 0;
    EXPECT_FALSE(multi_target_validate_config(&config));

    EXPECT_FALSE(multi_target_validate_config(nullptr));
}

TEST_F(MultiTargetTest, CreateWithCustomConfig) {
    multi_target_config_t config = multi_target_default_config();
    config.max_queue_size = 4;
    config.switch_hysteresis = 0.2f;

    dragonfly_multi_target_t custom = dragonfly_multi_target_create(&config);
    ASSERT_NE(custom, nullptr);
    dragonfly_multi_target_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MultiTargetTest, CreateAndDestroy) {
    dragonfly_multi_target_t m = dragonfly_multi_target_create(nullptr);
    ASSERT_NE(m, nullptr);
    dragonfly_multi_target_destroy(m);
}

TEST_F(MultiTargetTest, DestroyNull) {
    dragonfly_multi_target_destroy(nullptr);  // Should not crash
}

TEST_F(MultiTargetTest, Reset) {
    EXPECT_EQ(dragonfly_multi_target_reset(mt), 0);
}

//=============================================================================
// Target Queue Tests
//=============================================================================

TEST_F(MultiTargetTest, AddSingleTarget) {
    dragonfly_detection_t det = make_detection(1, 100, 0, 0, 0, 5.0f, 0.05f);
    dragonfly_self_state_t self = make_self_state(0, 0, 0);

    EXPECT_EQ(dragonfly_multi_target_update(mt, &det, &self), 0);

    multi_target_state_t state;
    EXPECT_EQ(dragonfly_multi_target_get_state(mt, &state), 0);
    EXPECT_EQ(state.num_targets, 1u);
}

TEST_F(MultiTargetTest, AddMultipleTargets) {
    dragonfly_self_state_t self = make_self_state(0, 0, 0);

    for (uint32_t i = 0; i < 4; i++) {
        dragonfly_detection_t det = make_detection(i + 1, 100.0f + i * 10, i * 5.0f, 0,
                                                    0.5f, 3.0f, 0.04f);
        EXPECT_EQ(dragonfly_multi_target_update(mt, &det, &self), 0);
    }

    multi_target_state_t state;
    EXPECT_EQ(dragonfly_multi_target_get_state(mt, &state), 0);
    EXPECT_EQ(state.num_targets, 4u);
}

TEST_F(MultiTargetTest, UpdateExistingTarget) {
    dragonfly_self_state_t self = make_self_state(0, 0, 0);
    dragonfly_detection_t det = make_detection(1, 100, 0, 0, 0, 5.0f, 0.05f);

    EXPECT_EQ(dragonfly_multi_target_update(mt, &det, &self), 0);

    // Update same target with new position
    det.position[0] = 95.0f;
    EXPECT_EQ(dragonfly_multi_target_update(mt, &det, &self), 0);

    multi_target_state_t state;
    EXPECT_EQ(dragonfly_multi_target_get_state(mt, &state), 0);
    EXPECT_EQ(state.num_targets, 1u);  // Still only one target
}

//=============================================================================
// Priority Tests
//=============================================================================

TEST_F(MultiTargetTest, PriorityByDistance) {
    dragonfly_self_state_t self = make_self_state(0, 0, 0);

    // Add far target first
    dragonfly_detection_t far = make_detection(1, 200, 0, 0, 0, 3.0f, 0.05f);
    EXPECT_EQ(dragonfly_multi_target_update(mt, &far, &self), 0);

    // Add close target
    dragonfly_detection_t close = make_detection(2, 50, 0, 0, 0, 3.0f, 0.05f);
    EXPECT_EQ(dragonfly_multi_target_update(mt, &close, &self), 0);

    // Evaluate to sort by priority
    EXPECT_EQ(dragonfly_multi_target_evaluate(mt, &self), 0);

    // Get sorted queue - close target should be first (highest priority)
    queued_target_t targets[4];
    uint32_t num_targets = 0;
    EXPECT_EQ(dragonfly_multi_target_get_queue(mt, targets, 4, &num_targets), 0);
    EXPECT_EQ(num_targets, 2u);
    EXPECT_EQ(targets[0].id, 2u);  // Close target should be first (highest priority)
}

TEST_F(MultiTargetTest, GetTargetById) {
    dragonfly_self_state_t self = make_self_state(0, 0, 0);
    dragonfly_detection_t det = make_detection(42, 100, 50, 0, 0.5f, 4.0f, 0.04f);

    EXPECT_EQ(dragonfly_multi_target_update(mt, &det, &self), 0);

    queued_target_t target;
    EXPECT_EQ(dragonfly_multi_target_get_by_id(mt, 42, &target), 0);
    EXPECT_EQ(target.id, 42u);
}

//=============================================================================
// Target Switching Tests
//=============================================================================

TEST_F(MultiTargetTest, SetPrimary) {
    dragonfly_self_state_t self = make_self_state(0, 0, 0);

    dragonfly_detection_t det1 = make_detection(1, 100, 0, 0, 0, 3.0f, 0.05f);
    dragonfly_detection_t det2 = make_detection(2, 80, 0, 0, 0, 3.0f, 0.05f);

    EXPECT_EQ(dragonfly_multi_target_update(mt, &det1, &self), 0);
    EXPECT_EQ(dragonfly_multi_target_update(mt, &det2, &self), 0);

    EXPECT_EQ(dragonfly_multi_target_set_primary(mt, 1), 0);

    multi_target_state_t state;
    EXPECT_EQ(dragonfly_multi_target_get_state(mt, &state), 0);
    EXPECT_EQ(state.primary_target_id, 1u);
}

TEST_F(MultiTargetTest, SwitchTarget) {
    dragonfly_self_state_t self = make_self_state(0, 0, 0);

    dragonfly_detection_t det1 = make_detection(1, 100, 0, 0, 0, 3.0f, 0.05f);
    dragonfly_detection_t det2 = make_detection(2, 80, 0, 0, 0, 3.0f, 0.05f);

    EXPECT_EQ(dragonfly_multi_target_update(mt, &det1, &self), 0);
    EXPECT_EQ(dragonfly_multi_target_update(mt, &det2, &self), 0);
    EXPECT_EQ(dragonfly_multi_target_set_primary(mt, 1), 0);

    switch_event_t event;
    EXPECT_EQ(dragonfly_multi_target_switch(mt, SWITCH_TARGET_ESCAPED, &event), 0);
    EXPECT_EQ(event.from_target_id, 1u);
    EXPECT_EQ(event.reason, SWITCH_TARGET_ESCAPED);
}

TEST_F(MultiTargetTest, GetBackup) {
    dragonfly_self_state_t self = make_self_state(0, 0, 0);

    dragonfly_detection_t det1 = make_detection(1, 100, 0, 0, 0, 3.0f, 0.05f);
    dragonfly_detection_t det2 = make_detection(2, 80, 0, 0, 0, 3.0f, 0.05f);

    EXPECT_EQ(dragonfly_multi_target_update(mt, &det1, &self), 0);
    EXPECT_EQ(dragonfly_multi_target_update(mt, &det2, &self), 0);
    EXPECT_EQ(dragonfly_multi_target_set_primary(mt, 1), 0);

    queued_target_t backup;
    EXPECT_EQ(dragonfly_multi_target_get_backup(mt, &backup), 0);
    EXPECT_NE(backup.id, 1u);  // Backup should not be primary
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MultiTargetTest, GetStats) {
    multi_target_stats_t stats;
    EXPECT_EQ(dragonfly_multi_target_get_stats(mt, &stats), 0);
    EXPECT_EQ(stats.targets_queued, 0u);
}

TEST_F(MultiTargetTest, StatsAfterOperations) {
    dragonfly_self_state_t self = make_self_state(0, 0, 0);
    dragonfly_detection_t det = make_detection(1, 100, 0, 0, 0, 5.0f, 0.05f);
    dragonfly_multi_target_update(mt, &det, &self);

    multi_target_stats_t stats;
    dragonfly_multi_target_get_stats(mt, &stats);
    EXPECT_GE(stats.targets_queued, 1u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(MultiTargetTest, NullPointerHandling) {
    dragonfly_detection_t det = make_detection(1, 100, 0, 0, 0, 5.0f, 0.05f);
    dragonfly_self_state_t self = make_self_state(0, 0, 0);

    EXPECT_EQ(dragonfly_multi_target_update(nullptr, &det, &self), -1);
    EXPECT_EQ(dragonfly_multi_target_update(mt, nullptr, &self), -1);
    EXPECT_EQ(dragonfly_multi_target_update(mt, &det, nullptr), -1);
    EXPECT_EQ(dragonfly_multi_target_get_state(nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_multi_target_get_primary(nullptr, nullptr), -1);
}
