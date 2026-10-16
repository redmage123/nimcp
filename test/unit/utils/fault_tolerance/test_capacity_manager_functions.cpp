/**
 * @file test_capacity_manager_functions.cpp
 * @brief Unit tests for Phase 5.8 Dynamic Capacity Management
 * @date 2026-01-18
 *
 * Tests for capacity manager creation, configuration, and basic operations.
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/fault_tolerance/nimcp_capacity_manager.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for capacity manager unit tests
 */
class CapacityManagerTest : public ::testing::Test {
protected:
    capacity_manager_t* cm = nullptr;

    void SetUp() override {
        capacity_config_t config = {};
        capacity_config_default(&config);
        config.initial_capacity = 100;
        config.max_capacity = 1000;

        int result = capacity_manager_create(&cm, &config, "test_module");
        ASSERT_EQ(result, 0);
        ASSERT_NE(cm, nullptr);
    }

    void TearDown() override {
        if (cm) {
            capacity_manager_destroy(cm);
            cm = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Default Tests
 * ============================================================================ */

TEST(CapacityConfigTest, DefaultValues) {
    capacity_config_t config = {};
    capacity_config_default(&config);

    EXPECT_EQ(config.initial_capacity, 512u);
    EXPECT_EQ(config.max_capacity, 0u);  /* Unlimited */
    EXPECT_FLOAT_EQ(config.growth_factor, 2.0f);
    EXPECT_FLOAT_EQ(config.shrink_threshold, 0.25f);
    EXPECT_FLOAT_EQ(config.warning_threshold, 0.9f);
    EXPECT_FLOAT_EQ(config.elevated_threshold, 0.75f);
    EXPECT_TRUE(config.enable_auto_expand);
    EXPECT_FALSE(config.enable_auto_shrink);
    EXPECT_TRUE(config.enable_immune_cleanup);
    EXPECT_TRUE(config.enable_trend_analysis);
}

TEST(CapacityConfigTest, DefaultNullSafe) {
    /* Should not crash with NULL */
    capacity_config_default(nullptr);
}

/* ============================================================================
 * Creation and Destruction Tests
 * ============================================================================ */

TEST(CapacityManagerCreateTest, WithDefaults) {
    capacity_manager_t* cm = nullptr;

    int result = capacity_manager_create(&cm, nullptr, "default_test");
    EXPECT_EQ(result, 0);
    ASSERT_NE(cm, nullptr);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.capacity, 512u);  /* Default initial */
    EXPECT_EQ(stats.current_count, 0u);

    capacity_manager_destroy(cm);
}

TEST(CapacityManagerCreateTest, WithCustomConfig) {
    capacity_config_t config = {};
    capacity_config_default(&config);
    config.initial_capacity = 256;
    config.max_capacity = 4096;

    capacity_manager_t* cm = nullptr;
    int result = capacity_manager_create(&cm, &config, "custom_test");
    EXPECT_EQ(result, 0);
    ASSERT_NE(cm, nullptr);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.capacity, 256u);

    capacity_manager_destroy(cm);
}

TEST(CapacityManagerCreateTest, NullOutput) {
    capacity_config_t config = {};
    capacity_config_default(&config);

    int result = capacity_manager_create(nullptr, &config, "test");
    EXPECT_EQ(result, -1);
}

TEST(CapacityManagerInitTest, InPlace) {
    capacity_manager_t cm;
    capacity_config_t config = {};
    capacity_config_default(&config);
    config.initial_capacity = 64;

    int result = capacity_manager_init(&cm, &config, "inplace_test");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(cm.magic, CAPACITY_MANAGER_MAGIC);

    capacity_stats_t stats;
    capacity_manager_get_stats(&cm, &stats);
    EXPECT_EQ(stats.capacity, 64u);
}

TEST(CapacityManagerInitTest, NullManager) {
    capacity_config_t config = {};
    int result = capacity_manager_init(nullptr, &config, "test");
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Slot Request/Release Tests
 * ============================================================================ */

TEST_F(CapacityManagerTest, RequestSlot) {
    int result = capacity_manager_request_slot(cm);
    EXPECT_EQ(result, 0);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.current_count, 1u);
}

TEST_F(CapacityManagerTest, RequestMultipleSlots) {
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(capacity_manager_request_slot(cm), 0);
    }

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.current_count, 50u);
}

TEST_F(CapacityManagerTest, ReleaseSlot) {
    capacity_manager_request_slot(cm);
    capacity_manager_request_slot(cm);

    int result = capacity_manager_release_slot(cm);
    EXPECT_EQ(result, 0);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.current_count, 1u);
}

TEST_F(CapacityManagerTest, ReleaseFromEmpty) {
    /* Should fail when count is 0 */
    int result = capacity_manager_release_slot(cm);
    EXPECT_EQ(result, -1);
}

TEST_F(CapacityManagerTest, RequestSlotNullManager) {
    int result = capacity_manager_request_slot(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Utilization and Level Tests
 * ============================================================================ */

TEST_F(CapacityManagerTest, GetUtilization) {
    float util = capacity_manager_get_utilization(cm);
    EXPECT_FLOAT_EQ(util, 0.0f);

    for (int i = 0; i < 50; i++) {
        capacity_manager_request_slot(cm);
    }

    util = capacity_manager_get_utilization(cm);
    EXPECT_FLOAT_EQ(util, 0.5f);  /* 50/100 */
}

TEST_F(CapacityManagerTest, GetLevel) {
    capacity_level_t level = capacity_manager_get_level(cm);
    EXPECT_EQ(level, CAPACITY_LEVEL_NORMAL);

    /* Fill to 76% - elevated */
    for (int i = 0; i < 76; i++) {
        capacity_manager_request_slot(cm);
    }
    level = capacity_manager_get_level(cm);
    EXPECT_EQ(level, CAPACITY_LEVEL_ELEVATED);

    /* Fill to 91% - warning */
    for (int i = 76; i < 91; i++) {
        capacity_manager_request_slot(cm);
    }
    level = capacity_manager_get_level(cm);
    EXPECT_EQ(level, CAPACITY_LEVEL_WARNING);

    /* Fill to 100% - critical */
    for (int i = 91; i < 100; i++) {
        capacity_manager_request_slot(cm);
    }
    level = capacity_manager_get_level(cm);
    EXPECT_EQ(level, CAPACITY_LEVEL_CRITICAL);
}

TEST_F(CapacityManagerTest, IsFull) {
    EXPECT_FALSE(capacity_manager_is_full(cm));

    for (int i = 0; i < 100; i++) {
        capacity_manager_request_slot(cm);
    }

    EXPECT_TRUE(capacity_manager_is_full(cm));
}

/* ============================================================================
 * Expansion Tests
 * ============================================================================ */

TEST_F(CapacityManagerTest, TriggerExpand) {
    int result = capacity_manager_trigger_expand(cm);
    EXPECT_EQ(result, 0);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.capacity, 200u);  /* 100 * 2.0 */
    EXPECT_EQ(stats.expansions, 1u);
}

TEST_F(CapacityManagerTest, ExpandAtMaxCapacity) {
    /* Fill up to max */
    while (capacity_manager_trigger_expand(cm) == 0) {
        capacity_stats_t stats;
        capacity_manager_get_stats(cm, &stats);
        if (stats.capacity >= 1000) break;
    }

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.capacity, 1000u);

    /* Should fail to expand further */
    int result = capacity_manager_trigger_expand(cm);
    EXPECT_EQ(result, -1);
}

TEST_F(CapacityManagerTest, AutoExpandOnFull) {
    /* Fill to capacity */
    for (int i = 0; i < 100; i++) {
        capacity_manager_request_slot(cm);
    }

    /* Next request should trigger auto-expand */
    int result = capacity_manager_request_slot(cm);
    EXPECT_EQ(result, 0);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_GT(stats.capacity, 100u);
    EXPECT_EQ(stats.current_count, 101u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CapacityManagerTest, GetStats) {
    capacity_stats_t stats = {};
    capacity_manager_get_stats(cm, &stats);

    EXPECT_EQ(stats.current_count, 0u);
    EXPECT_EQ(stats.capacity, 100u);
    EXPECT_FLOAT_EQ(stats.utilization, 0.0f);
    EXPECT_EQ(stats.level, CAPACITY_LEVEL_NORMAL);
    EXPECT_EQ(stats.expansions, 0u);
    EXPECT_EQ(stats.failed_allocations, 0u);
}

TEST_F(CapacityManagerTest, GetStatsNullStats) {
    capacity_manager_get_stats(cm, nullptr);  /* Should not crash */
}

TEST_F(CapacityManagerTest, PeakTracking) {
    for (int i = 0; i < 75; i++) {
        capacity_manager_request_slot(cm);
    }
    for (int i = 0; i < 25; i++) {
        capacity_manager_release_slot(cm);
    }

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);

    EXPECT_EQ(stats.current_count, 50u);
    EXPECT_EQ(stats.peak_count, 75u);
    EXPECT_FLOAT_EQ(stats.peak_utilization, 0.75f);
}

TEST_F(CapacityManagerTest, ResetStats) {
    capacity_manager_trigger_expand(cm);
    for (int i = 0; i < 50; i++) {
        capacity_manager_request_slot(cm);
    }

    capacity_manager_reset_stats(cm);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.expansions, 0u);
    EXPECT_FLOAT_EQ(stats.peak_utilization, 0.0f);
    EXPECT_EQ(stats.peak_count, 0u);
}

/* ============================================================================
 * Set Count Tests
 * ============================================================================ */

TEST_F(CapacityManagerTest, SetCount) {
    capacity_manager_set_count(cm, 42);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.current_count, 42u);
}

TEST_F(CapacityManagerTest, SetCountNull) {
    capacity_manager_set_count(nullptr, 10);  /* Should not crash */
}

/* ============================================================================
 * Check Function Tests
 * ============================================================================ */

TEST_F(CapacityManagerTest, CheckUpdatesCount) {
    capacity_level_t level = capacity_manager_check(cm, 50);
    EXPECT_EQ(level, CAPACITY_LEVEL_NORMAL);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.current_count, 50u);
}

TEST_F(CapacityManagerTest, CheckTriggersAction) {
    /* Check at critical level should trigger expand */
    capacity_level_t level = capacity_manager_check(cm, 100);
    EXPECT_LT(level, CAPACITY_LEVEL_CRITICAL);  /* Should have expanded */

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_GT(stats.capacity, 100u);
}

/* ============================================================================
 * Enum Value Tests
 * ============================================================================ */

TEST(CapacityLevelTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_NORMAL), 0);
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_ELEVATED), 1);
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_WARNING), 2);
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_CRITICAL), 3);
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_EXCEEDED), 4);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
