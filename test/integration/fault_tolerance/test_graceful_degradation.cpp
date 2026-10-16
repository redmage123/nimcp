/**
 * @file test_graceful_degradation.cpp
 * @brief Tests for graceful degradation framework
 *
 * WHAT: Verify degradation tier transitions, features, resources, load shedding
 * WHY:  Graceful degradation maintains core functionality under resource pressure
 * HOW:  Test lifecycle, tier management, feature registration, resource tracking
 *
 * Function signatures tested (from include/utils/fault_tolerance/nimcp_graceful_degradation.h):
 *   gd_config_t gd_default_config(void);
 *   gd_context_t* gd_create(const gd_config_t* config);
 *   void gd_destroy(gd_context_t* ctx);
 *   gd_tier_t gd_get_current_tier(gd_context_t* ctx);
 *   bool gd_set_tier(gd_context_t* ctx, gd_tier_t tier, const char* reason);
 *   uint32_t gd_register_feature(gd_context_t* ctx, const gd_feature_t* feature);
 *   bool gd_unregister_feature(gd_context_t* ctx, uint32_t feature_id);
 *   bool gd_is_feature_enabled(gd_context_t* ctx, uint32_t feature_id);
 *   bool gd_set_feature_enabled(gd_context_t* ctx, uint32_t feature_id, bool enabled);
 *   bool gd_update_resource(gd_context_t* ctx, gd_resource_t resource, float usage);
 *   float gd_get_resource_usage(gd_context_t* ctx, gd_resource_t resource);
 *   bool gd_set_resource_budget(gd_context_t* ctx, const gd_resource_budget_t* budget);
 *   bool gd_start_load_shedding(gd_context_t* ctx, float rate, gd_priority_t min, uint64_t dur);
 *   bool gd_stop_load_shedding(gd_context_t* ctx);
 *   bool gd_should_accept_request(gd_context_t* ctx, gd_priority_t priority);
 *   bool gd_get_stats(gd_context_t* ctx, gd_stats_t* stats);
 *   void gd_reset_stats(gd_context_t* ctx);
 *
 * String conversion:
 *   const char* gd_tier_to_string(gd_tier_t tier);
 *   const char* gd_priority_to_string(gd_priority_t priority);
 *   const char* gd_resource_to_string(gd_resource_t resource);
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "utils/fault_tolerance/nimcp_graceful_degradation.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GracefulDegradationTest : public ::testing::Test {
protected:
    gd_context_t* ctx = nullptr;

    void SetUp() override {
        gd_config_t config = gd_default_config();
        config.enable_auto_degradation = false; // Manual tier control for testing
        config.enable_load_shedding = true;
        config.enable_quality_reduction = true;
        config.initial_tier = GD_TIER_FULL;
        ctx = gd_create(&config);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        gd_destroy(ctx);
        ctx = nullptr;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST(GDLifecycleTest, CreateWithDefaults) {
    gd_config_t config = gd_default_config();
    gd_context_t* c = gd_create(&config);
    EXPECT_NE(c, nullptr);
    gd_destroy(c);
}

TEST(GDLifecycleTest, CreateWithNullConfig) {
    gd_context_t* c = gd_create(nullptr);
    // May return NULL or use defaults
    if (c) {
        gd_destroy(c);
    }
    SUCCEED();
}

TEST(GDLifecycleTest, DestroyNull) {
    gd_destroy(nullptr);
    SUCCEED() << "Destroying NULL context did not crash";
}

TEST(GDLifecycleTest, DefaultConfigValid) {
    gd_config_t config = gd_default_config();
    EXPECT_EQ(config.initial_tier, GD_TIER_FULL);
    EXPECT_GE(config.check_interval_ms, 0u);
}

/* ============================================================================
 * Tier Transition Tests
 * ============================================================================ */

TEST_F(GracefulDegradationTest, InitialTierIsFull) {
    EXPECT_EQ(gd_get_current_tier(ctx), GD_TIER_FULL);
}

TEST_F(GracefulDegradationTest, SetTierStandard) {
    bool ok = gd_set_tier(ctx, GD_TIER_STANDARD, "test downgrade");
    EXPECT_TRUE(ok);
    EXPECT_EQ(gd_get_current_tier(ctx), GD_TIER_STANDARD);
}

TEST_F(GracefulDegradationTest, SetTierReduced) {
    bool ok = gd_set_tier(ctx, GD_TIER_REDUCED, "test reduced");
    EXPECT_TRUE(ok);
    EXPECT_EQ(gd_get_current_tier(ctx), GD_TIER_REDUCED);
}

TEST_F(GracefulDegradationTest, SetTierMinimal) {
    bool ok = gd_set_tier(ctx, GD_TIER_MINIMAL, "test minimal");
    EXPECT_TRUE(ok);
    EXPECT_EQ(gd_get_current_tier(ctx), GD_TIER_MINIMAL);
}

TEST_F(GracefulDegradationTest, SetTierEmergency) {
    bool ok = gd_set_tier(ctx, GD_TIER_EMERGENCY, "test emergency");
    EXPECT_TRUE(ok);
    EXPECT_EQ(gd_get_current_tier(ctx), GD_TIER_EMERGENCY);
}

TEST_F(GracefulDegradationTest, UpgradeTier) {
    // Downgrade first
    gd_set_tier(ctx, GD_TIER_REDUCED, "downgrade");
    EXPECT_EQ(gd_get_current_tier(ctx), GD_TIER_REDUCED);

    // Upgrade back
    bool ok = gd_set_tier(ctx, GD_TIER_FULL, "upgrade");
    EXPECT_TRUE(ok);
    EXPECT_EQ(gd_get_current_tier(ctx), GD_TIER_FULL);
}

/* ============================================================================
 * Feature Priority Ordering Tests
 * ============================================================================ */

TEST_F(GracefulDegradationTest, RegisterFeature) {
    gd_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    strncpy(feature.name, "test_feature", sizeof(feature.name) - 1);
    feature.priority = GD_PRIORITY_MEDIUM;
    feature.minimum_tier = GD_TIER_STANDARD;
    feature.is_enabled = true;
    feature.can_degrade = true;
    feature.current_quality = 100.0f;
    feature.min_quality = 50.0f;

    uint32_t id = gd_register_feature(ctx, &feature);
    EXPECT_NE(id, 0u);

    // Feature should be enabled
    EXPECT_TRUE(gd_is_feature_enabled(ctx, id));

    // Unregister
    bool ok = gd_unregister_feature(ctx, id);
    EXPECT_TRUE(ok);
}

TEST_F(GracefulDegradationTest, FeatureEnableDisable) {
    gd_feature_t feature;
    memset(&feature, 0, sizeof(feature));
    strncpy(feature.name, "toggle_feature", sizeof(feature.name) - 1);
    feature.priority = GD_PRIORITY_LOW;
    feature.is_enabled = true;

    uint32_t id = gd_register_feature(ctx, &feature);
    ASSERT_NE(id, 0u);

    // Disable
    bool ok = gd_set_feature_enabled(ctx, id, false);
    EXPECT_TRUE(ok);
    EXPECT_FALSE(gd_is_feature_enabled(ctx, id));

    // Re-enable
    ok = gd_set_feature_enabled(ctx, id, true);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(gd_is_feature_enabled(ctx, id));

    gd_unregister_feature(ctx, id);
}

TEST_F(GracefulDegradationTest, PriorityOrdering) {
    // Verify priority enum values are ordered
    EXPECT_LT(GD_PRIORITY_CRITICAL, GD_PRIORITY_HIGH);
    EXPECT_LT(GD_PRIORITY_HIGH, GD_PRIORITY_MEDIUM);
    EXPECT_LT(GD_PRIORITY_MEDIUM, GD_PRIORITY_LOW);
    EXPECT_LT(GD_PRIORITY_LOW, GD_PRIORITY_OPTIONAL);
}

/* ============================================================================
 * Resource Budget Tracking Tests
 * ============================================================================ */

TEST_F(GracefulDegradationTest, UpdateResourceUsage) {
    bool ok = gd_update_resource(ctx, GD_RESOURCE_CPU, 50.0f);
    EXPECT_TRUE(ok);

    float usage = gd_get_resource_usage(ctx, GD_RESOURCE_CPU);
    EXPECT_FLOAT_EQ(usage, 50.0f);
}

TEST_F(GracefulDegradationTest, UpdateMultipleResources) {
    gd_update_resource(ctx, GD_RESOURCE_CPU, 30.0f);
    gd_update_resource(ctx, GD_RESOURCE_MEMORY, 60.0f);
    gd_update_resource(ctx, GD_RESOURCE_GPU, 80.0f);

    EXPECT_FLOAT_EQ(gd_get_resource_usage(ctx, GD_RESOURCE_CPU), 30.0f);
    EXPECT_FLOAT_EQ(gd_get_resource_usage(ctx, GD_RESOURCE_MEMORY), 60.0f);
    EXPECT_FLOAT_EQ(gd_get_resource_usage(ctx, GD_RESOURCE_GPU), 80.0f);
}

TEST_F(GracefulDegradationTest, SetResourceBudget) {
    gd_resource_budget_t budget;
    memset(&budget, 0, sizeof(budget));
    budget.type = GD_RESOURCE_MEMORY;
    budget.warning_threshold = 70.0f;
    budget.critical_threshold = 90.0f;

    bool ok = gd_set_resource_budget(ctx, &budget);
    EXPECT_TRUE(ok);
}

/* ============================================================================
 * Load Shedding Configuration Tests
 * ============================================================================ */

TEST_F(GracefulDegradationTest, StartStopLoadShedding) {
    bool ok = gd_start_load_shedding(ctx, 50.0f, GD_PRIORITY_MEDIUM, 5000);
    EXPECT_TRUE(ok);

    ok = gd_stop_load_shedding(ctx);
    EXPECT_TRUE(ok);
}

TEST_F(GracefulDegradationTest, LoadSheddingAcceptsHighPriority) {
    gd_start_load_shedding(ctx, 50.0f, GD_PRIORITY_MEDIUM, 5000);

    // Critical requests should always be accepted
    EXPECT_TRUE(gd_should_accept_request(ctx, GD_PRIORITY_CRITICAL));

    gd_stop_load_shedding(ctx);
}

TEST_F(GracefulDegradationTest, LoadSheddingStatus) {
    gd_start_load_shedding(ctx, 30.0f, GD_PRIORITY_LOW, 10000);

    gd_load_shed_config_t shed_config;
    bool is_active = gd_get_load_shed_status(ctx, &shed_config);
    EXPECT_TRUE(is_active);
    EXPECT_TRUE(shed_config.enabled);
    EXPECT_FLOAT_EQ(shed_config.shed_rate, 30.0f);

    gd_stop_load_shedding(ctx);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(GracefulDegradationTest, GetStats) {
    gd_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    bool ok = gd_get_stats(ctx, &stats);
    EXPECT_TRUE(ok);
}

TEST_F(GracefulDegradationTest, ResetStats) {
    // Perform some operations
    gd_set_tier(ctx, GD_TIER_STANDARD, "test");
    gd_set_tier(ctx, GD_TIER_FULL, "restore");

    gd_reset_stats(ctx);

    gd_stats_t stats;
    bool ok = gd_get_stats(ctx, &stats);
    EXPECT_TRUE(ok);
    EXPECT_EQ(stats.total_transitions, 0u);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST(GDStringTest, TierToString) {
    const char* s;

    s = gd_tier_to_string(GD_TIER_FULL);
    ASSERT_NE(s, nullptr);
    EXPECT_GT(strlen(s), 0u);

    s = gd_tier_to_string(GD_TIER_STANDARD);
    ASSERT_NE(s, nullptr);

    s = gd_tier_to_string(GD_TIER_REDUCED);
    ASSERT_NE(s, nullptr);

    s = gd_tier_to_string(GD_TIER_MINIMAL);
    ASSERT_NE(s, nullptr);

    s = gd_tier_to_string(GD_TIER_EMERGENCY);
    ASSERT_NE(s, nullptr);
}

TEST(GDStringTest, PriorityToString) {
    const char* s;

    s = gd_priority_to_string(GD_PRIORITY_CRITICAL);
    ASSERT_NE(s, nullptr);

    s = gd_priority_to_string(GD_PRIORITY_OPTIONAL);
    ASSERT_NE(s, nullptr);
}

TEST(GDStringTest, ResourceToString) {
    const char* s;

    s = gd_resource_to_string(GD_RESOURCE_CPU);
    ASSERT_NE(s, nullptr);

    s = gd_resource_to_string(GD_RESOURCE_MEMORY);
    ASSERT_NE(s, nullptr);

    s = gd_resource_to_string(GD_RESOURCE_GPU);
    ASSERT_NE(s, nullptr);
}

/* ============================================================================
 * Enum Tests
 * ============================================================================ */

TEST(GDEnumTest, TierValues) {
    EXPECT_EQ(GD_TIER_FULL, 0);
    EXPECT_LT(GD_TIER_FULL, GD_TIER_STANDARD);
    EXPECT_LT(GD_TIER_STANDARD, GD_TIER_REDUCED);
    EXPECT_LT(GD_TIER_REDUCED, GD_TIER_MINIMAL);
    EXPECT_LT(GD_TIER_MINIMAL, GD_TIER_EMERGENCY);
}

TEST(GDEnumTest, ResourceValues) {
    EXPECT_EQ(GD_RESOURCE_CPU, 0);
    EXPECT_LT(GD_RESOURCE_COUNT, GD_MAX_RESOURCES);
}
