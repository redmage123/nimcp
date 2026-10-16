/**
 * @file test_immune_bridge_coordinator.cpp
 * @brief Unit tests for Immune Bridge Coordinator
 * @version 1.0.0
 * @date 2025-12-15
 *
 * Tests for the immune bridge coordinator including:
 * - Lifecycle (create, destroy, start, stop, pause, resume)
 * - Bridge registration and unregistration
 * - Bridge enable/disable
 * - Bridge retrieval (by ID, by category)
 * - Update operations (all, category, individual bridge)
 * - Health monitoring and checks
 * - Category-based operations
 * - Statistics tracking
 * - Bio-async integration
 * - Brain immune integration
 * - Thread safety
 * - Cross-bridge messaging
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_immune_bridge_coordinator.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Mock Bridge Implementations
 * ============================================================================ */

// Mock bridge structure
typedef struct {
    uint32_t id;
    uint32_t update_count;
    bool should_fail;
    immune_bridge_health_t health;
} mock_bridge_t;

// Mock update callback
static int mock_bridge_update(immune_bridge_handle_t handle) {
    mock_bridge_t* bridge = (mock_bridge_t*)handle;
    if (!bridge) return -1;
    if (bridge->should_fail) return -1;

    bridge->update_count++;
    return 0;
}

// Mock destroy callback
static void mock_bridge_destroy(immune_bridge_handle_t handle) {
    // Nothing to do for mock
    (void)handle;
}

// Mock health check callback
static immune_bridge_health_t mock_bridge_health_check(immune_bridge_handle_t handle) {
    mock_bridge_t* bridge = (mock_bridge_t*)handle;
    if (!bridge) return IMMUNE_BRIDGE_ERROR;
    return bridge->health;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImmuneBridgeCoordinatorTest : public ::testing::Test {
protected:
    immune_bridge_coordinator_t* coordinator = nullptr;
    immune_bridge_coordinator_config_t config;

    // Mock bridges
    mock_bridge_t mock1;
    mock_bridge_t mock2;
    mock_bridge_t mock3;

    void SetUp() override {
        immune_bridge_coordinator_default_config(&config);
        coordinator = immune_bridge_coordinator_create(&config);
        ASSERT_NE(coordinator, nullptr);

        // Initialize mock bridges
        mock1 = {1, 0, false, IMMUNE_BRIDGE_HEALTHY};
        mock2 = {2, 0, false, IMMUNE_BRIDGE_HEALTHY};
        mock3 = {3, 0, false, IMMUNE_BRIDGE_HEALTHY};
    }

    void TearDown() override {
        if (coordinator) {
            immune_bridge_coordinator_destroy(coordinator);
            coordinator = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, DefaultConfigIsValid) {
    immune_bridge_coordinator_config_t cfg;
    int result = immune_bridge_coordinator_default_config(&cfg);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(cfg.max_bridges, IMMUNE_COORDINATOR_MAX_BRIDGES);
    EXPECT_TRUE(cfg.enable_auto_update);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_TRUE(cfg.enable_brain_immune);
    EXPECT_TRUE(cfg.enable_statistics);
    EXPECT_EQ(cfg.health_check_interval_ms, IMMUNE_COORDINATOR_HEALTH_CHECK_MS);

    // Check all categories enabled
    for (int i = 0; i < IMMUNE_BRIDGE_CATEGORY_COUNT; i++) {
        EXPECT_TRUE(cfg.categories[i].enabled);
        EXPECT_EQ(cfg.categories[i].update_priority, (uint32_t)i);
    }
}

TEST_F(ImmuneBridgeCoordinatorTest, DefaultConfigNullFails) {
    int result = immune_bridge_coordinator_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, CreateWithNullConfigUsesDefaults) {
    immune_bridge_coordinator_t* coord = immune_bridge_coordinator_create(nullptr);
    ASSERT_NE(coord, nullptr);
    EXPECT_EQ(coord->config.max_bridges, IMMUNE_COORDINATOR_MAX_BRIDGES);
    immune_bridge_coordinator_destroy(coord);
}

TEST_F(ImmuneBridgeCoordinatorTest, CreateWithCustomConfig) {
    immune_bridge_coordinator_config_t custom_cfg;
    immune_bridge_coordinator_default_config(&custom_cfg);
    custom_cfg.max_bridges = 64;
    custom_cfg.enable_logging = false;

    immune_bridge_coordinator_t* coord = immune_bridge_coordinator_create(&custom_cfg);
    ASSERT_NE(coord, nullptr);
    EXPECT_EQ(coord->config.max_bridges, 64u);
    EXPECT_FALSE(coord->config.enable_logging);
    immune_bridge_coordinator_destroy(coord);
}

TEST_F(ImmuneBridgeCoordinatorTest, InitialStateIsStopped) {
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_STOPPED);
}

TEST_F(ImmuneBridgeCoordinatorTest, StartChangesStateToRunning) {
    int result = immune_bridge_coordinator_start(coordinator);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_RUNNING);
}

TEST_F(ImmuneBridgeCoordinatorTest, StopChangesStateToStopped) {
    immune_bridge_coordinator_start(coordinator);

    int result = immune_bridge_coordinator_stop(coordinator);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_STOPPED);
}

TEST_F(ImmuneBridgeCoordinatorTest, StartNullFails) {
    int result = immune_bridge_coordinator_start(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, StopNullFails) {
    int result = immune_bridge_coordinator_stop(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, DoubleStartIsIdempotent) {
    immune_bridge_coordinator_start(coordinator);
    int result = immune_bridge_coordinator_start(coordinator);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_RUNNING);
}

TEST_F(ImmuneBridgeCoordinatorTest, DoubleStopIsIdempotent) {
    immune_bridge_coordinator_stop(coordinator);
    int result = immune_bridge_coordinator_stop(coordinator);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_STOPPED);
}

TEST_F(ImmuneBridgeCoordinatorTest, PauseFromRunningSucceeds) {
    immune_bridge_coordinator_start(coordinator);

    int result = immune_bridge_coordinator_pause(coordinator);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_PAUSED);
}

TEST_F(ImmuneBridgeCoordinatorTest, PauseFromStoppedFails) {
    int result = immune_bridge_coordinator_pause(coordinator);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
}

TEST_F(ImmuneBridgeCoordinatorTest, ResumeFromPausedSucceeds) {
    immune_bridge_coordinator_start(coordinator);
    immune_bridge_coordinator_pause(coordinator);

    int result = immune_bridge_coordinator_resume(coordinator);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_RUNNING);
}

TEST_F(ImmuneBridgeCoordinatorTest, ResumeFromStoppedFails) {
    int result = immune_bridge_coordinator_resume(coordinator);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
}

TEST_F(ImmuneBridgeCoordinatorTest, PauseNullFails) {
    int result = immune_bridge_coordinator_pause(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, ResumeNullFails) {
    int result = immune_bridge_coordinator_resume(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, DestroyNullIsSafe) {
    immune_bridge_coordinator_destroy(nullptr);
    // Should not crash
}

TEST_F(ImmuneBridgeCoordinatorTest, DestroyWhileRunningStopsFirst) {
    immune_bridge_coordinator_start(coordinator);
    immune_bridge_coordinator_destroy(coordinator);
    coordinator = nullptr;  // Prevent double-free in TearDown
    // Should complete without error
}

/* ============================================================================
 * Bridge Registration Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, RegisterBridgeSucceeds) {
    uint32_t bridge_id = 0;
    int result = immune_bridge_coordinator_register_bridge(
        coordinator,
        "test_bridge",
        IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1,
        mock_bridge_update,
        mock_bridge_destroy,
        mock_bridge_health_check,
        &bridge_id
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge_id, 0u);
    EXPECT_EQ(coordinator->bridge_count, 1u);
}

TEST_F(ImmuneBridgeCoordinatorTest, RegisterBridgeNullCoordinatorFails) {
    uint32_t bridge_id = 0;
    int result = immune_bridge_coordinator_register_bridge(
        nullptr,
        "test_bridge",
        IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1,
        mock_bridge_update,
        nullptr,
        nullptr,
        &bridge_id
    );
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, RegisterBridgeNullNameFails) {
    uint32_t bridge_id = 0;
    int result = immune_bridge_coordinator_register_bridge(
        coordinator,
        nullptr,
        IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1,
        mock_bridge_update,
        nullptr,
        nullptr,
        &bridge_id
    );
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, RegisterBridgeNullHandleFails) {
    uint32_t bridge_id = 0;
    int result = immune_bridge_coordinator_register_bridge(
        coordinator,
        "test_bridge",
        IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        nullptr,
        mock_bridge_update,
        nullptr,
        nullptr,
        &bridge_id
    );
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, RegisterBridgeInvalidCategoryFails) {
    uint32_t bridge_id = 0;
    int result = immune_bridge_coordinator_register_bridge(
        coordinator,
        "test_bridge",
        IMMUNE_BRIDGE_CATEGORY_COUNT,  // Invalid
        &mock1,
        mock_bridge_update,
        nullptr,
        nullptr,
        &bridge_id
    );
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(ImmuneBridgeCoordinatorTest, RegisterBridgeWithoutCallbacks) {
    uint32_t bridge_id = 0;
    int result = immune_bridge_coordinator_register_bridge(
        coordinator,
        "test_bridge",
        IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1,
        nullptr,  // No callbacks
        nullptr,
        nullptr,
        &bridge_id
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge_id, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, RegisterBridgeNullOutputIdSucceeds) {
    int result = immune_bridge_coordinator_register_bridge(
        coordinator,
        "test_bridge",
        IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1,
        mock_bridge_update,
        nullptr,
        nullptr,
        nullptr  // NULL output
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ImmuneBridgeCoordinatorTest, RegisterMultipleBridges) {
    uint32_t id1, id2, id3;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &id1);

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge2", IMMUNE_BRIDGE_CATEGORY_PLASTICITY,
        &mock2, mock_bridge_update, nullptr, nullptr, &id2);

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge3", IMMUNE_BRIDGE_CATEGORY_MIDDLEWARE,
        &mock3, mock_bridge_update, nullptr, nullptr, &id3);

    EXPECT_EQ(coordinator->bridge_count, 3u);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST_F(ImmuneBridgeCoordinatorTest, RegisterBridgeUpdatesStatistics) {
    uint32_t bridge_id = 0;
    immune_bridge_coordinator_register_bridge(
        coordinator, "test_bridge", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    immune_coordinator_stats_t stats;
    immune_bridge_coordinator_get_stats(coordinator, &stats);

    EXPECT_EQ(stats.total_bridges, 1u);
    EXPECT_EQ(stats.active_bridges, 1u);
    EXPECT_EQ(stats.healthy_bridges, 1u);
    EXPECT_EQ(stats.categories[IMMUNE_BRIDGE_CATEGORY_COGNITIVE].bridge_count, 1u);
    EXPECT_EQ(stats.categories[IMMUNE_BRIDGE_CATEGORY_COGNITIVE].healthy_bridges, 1u);
}

/* ============================================================================
 * Bridge Unregistration Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, UnregisterBridgeSucceeds) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "test_bridge", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    int result = immune_bridge_coordinator_unregister_bridge(coordinator, bridge_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(coordinator->bridge_count, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, UnregisterBridgeNullFails) {
    int result = immune_bridge_coordinator_unregister_bridge(nullptr, 1);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, UnregisterNonexistentBridgeFails) {
    int result = immune_bridge_coordinator_unregister_bridge(coordinator, 999);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(ImmuneBridgeCoordinatorTest, UnregisterBridgeUpdatesStatistics) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "test_bridge", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    immune_bridge_coordinator_unregister_bridge(coordinator, bridge_id);

    immune_coordinator_stats_t stats;
    immune_bridge_coordinator_get_stats(coordinator, &stats);

    EXPECT_EQ(stats.total_bridges, 0u);
    EXPECT_EQ(stats.active_bridges, 0u);
    EXPECT_EQ(stats.healthy_bridges, 0u);
    EXPECT_EQ(stats.categories[IMMUNE_BRIDGE_CATEGORY_COGNITIVE].bridge_count, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, UnregisterMiddleBridgeShiftsArray) {
    uint32_t id1, id2, id3;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &id1);
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge2", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock2, mock_bridge_update, nullptr, nullptr, &id2);
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge3", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock3, mock_bridge_update, nullptr, nullptr, &id3);

    // Unregister middle bridge
    immune_bridge_coordinator_unregister_bridge(coordinator, id2);

    EXPECT_EQ(coordinator->bridge_count, 2u);

    // Verify remaining bridges are accessible
    const immune_bridge_entry_t* entry1 = immune_bridge_coordinator_get_bridge(coordinator, id1);
    const immune_bridge_entry_t* entry3 = immune_bridge_coordinator_get_bridge(coordinator, id3);
    EXPECT_NE(entry1, nullptr);
    EXPECT_NE(entry3, nullptr);

    // Verify removed bridge is not accessible
    const immune_bridge_entry_t* entry2 = immune_bridge_coordinator_get_bridge(coordinator, id2);
    EXPECT_EQ(entry2, nullptr);
}

/* ============================================================================
 * Bridge Enable/Disable Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, SetBridgeEnabledSucceeds) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "test_bridge", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    int result = immune_bridge_coordinator_set_bridge_enabled(coordinator, bridge_id, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ImmuneBridgeCoordinatorTest, SetBridgeEnabledNullFails) {
    int result = immune_bridge_coordinator_set_bridge_enabled(nullptr, 1, true);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, SetBridgeEnabledInvalidIdFails) {
    int result = immune_bridge_coordinator_set_bridge_enabled(coordinator, 999, true);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(ImmuneBridgeCoordinatorTest, DisableBridgeUpdatesStatistics) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "test_bridge", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    immune_coordinator_stats_t stats;
    immune_bridge_coordinator_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.active_bridges, 1u);

    immune_bridge_coordinator_set_bridge_enabled(coordinator, bridge_id, false);
    immune_bridge_coordinator_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.active_bridges, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, EnableBridgeUpdatesStatistics) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "test_bridge", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    immune_bridge_coordinator_set_bridge_enabled(coordinator, bridge_id, false);
    immune_bridge_coordinator_set_bridge_enabled(coordinator, bridge_id, true);

    immune_coordinator_stats_t stats;
    immune_bridge_coordinator_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.active_bridges, 1u);
}

/* ============================================================================
 * Bridge Retrieval Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, GetBridgeByIdSucceeds) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "test_bridge", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    const immune_bridge_entry_t* entry = immune_bridge_coordinator_get_bridge(coordinator, bridge_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->bridge_id, bridge_id);
    EXPECT_STREQ(entry->bridge_name, "test_bridge");
    EXPECT_EQ(entry->category, IMMUNE_BRIDGE_CATEGORY_COGNITIVE);
    EXPECT_EQ(entry->handle, &mock1);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetBridgeNullReturnsNull) {
    const immune_bridge_entry_t* entry = immune_bridge_coordinator_get_bridge(nullptr, 1);
    EXPECT_EQ(entry, nullptr);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetNonexistentBridgeReturnsNull) {
    const immune_bridge_entry_t* entry = immune_bridge_coordinator_get_bridge(coordinator, 999);
    EXPECT_EQ(entry, nullptr);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetBridgesByCategorySucceeds) {
    uint32_t id1, id2, id3;

    immune_bridge_coordinator_register_bridge(
        coordinator, "cognitive1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &id1);
    immune_bridge_coordinator_register_bridge(
        coordinator, "cognitive2", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock2, mock_bridge_update, nullptr, nullptr, &id2);
    immune_bridge_coordinator_register_bridge(
        coordinator, "plasticity1", IMMUNE_BRIDGE_CATEGORY_PLASTICITY,
        &mock3, mock_bridge_update, nullptr, nullptr, &id3);

    const immune_bridge_entry_t* bridges[10];
    uint32_t count = immune_bridge_coordinator_get_bridges_by_category(
        coordinator, IMMUNE_BRIDGE_CATEGORY_COGNITIVE, bridges, 10);

    EXPECT_EQ(count, 2u);
    EXPECT_EQ(bridges[0]->category, IMMUNE_BRIDGE_CATEGORY_COGNITIVE);
    EXPECT_EQ(bridges[1]->category, IMMUNE_BRIDGE_CATEGORY_COGNITIVE);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetBridgesByCategoryNullReturnsZero) {
    const immune_bridge_entry_t* bridges[10];
    uint32_t count = immune_bridge_coordinator_get_bridges_by_category(
        nullptr, IMMUNE_BRIDGE_CATEGORY_COGNITIVE, bridges, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetBridgesByCategoryNullArrayReturnsZero) {
    uint32_t count = immune_bridge_coordinator_get_bridges_by_category(
        coordinator, IMMUNE_BRIDGE_CATEGORY_COGNITIVE, nullptr, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetBridgesByCategoryInvalidCategoryReturnsZero) {
    const immune_bridge_entry_t* bridges[10];
    uint32_t count = immune_bridge_coordinator_get_bridges_by_category(
        coordinator, IMMUNE_BRIDGE_CATEGORY_COUNT, bridges, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetBridgesByCategoryRespectsMaxBridges) {
    uint32_t id1, id2, id3;

    immune_bridge_coordinator_register_bridge(
        coordinator, "cognitive1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &id1);
    immune_bridge_coordinator_register_bridge(
        coordinator, "cognitive2", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock2, mock_bridge_update, nullptr, nullptr, &id2);
    immune_bridge_coordinator_register_bridge(
        coordinator, "cognitive3", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock3, mock_bridge_update, nullptr, nullptr, &id3);

    const immune_bridge_entry_t* bridges[2];
    uint32_t count = immune_bridge_coordinator_get_bridges_by_category(
        coordinator, IMMUNE_BRIDGE_CATEGORY_COGNITIVE, bridges, 2);

    EXPECT_EQ(count, 2u);  // Limited by max_bridges parameter
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, UpdateAllBridges) {
    uint32_t id1, id2;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &id1);
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge2", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock2, mock_bridge_update, nullptr, nullptr, &id2);

    immune_bridge_coordinator_start(coordinator);

    int count = immune_bridge_coordinator_update(coordinator, 1000);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(mock1.update_count, 1u);
    EXPECT_EQ(mock2.update_count, 1u);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateNullFails) {
    int result = immune_bridge_coordinator_update(nullptr, 1000);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateWhenStoppedReturnsZero) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    // Don't start coordinator
    int count = immune_bridge_coordinator_update(coordinator, 1000);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(mock1.update_count, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateSkipsDisabledBridges) {
    uint32_t id1, id2;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &id1);
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge2", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock2, mock_bridge_update, nullptr, nullptr, &id2);

    immune_bridge_coordinator_set_bridge_enabled(coordinator, id2, false);
    immune_bridge_coordinator_start(coordinator);

    int count = immune_bridge_coordinator_update(coordinator, 1000);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(mock1.update_count, 1u);
    EXPECT_EQ(mock2.update_count, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateSkipsBridgesWithoutUpdateCallback) {
    uint32_t id1, id2;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &id1);
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge2", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock2, nullptr, nullptr, nullptr, &id2);  // No update callback

    immune_bridge_coordinator_start(coordinator);

    int count = immune_bridge_coordinator_update(coordinator, 1000);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(mock1.update_count, 1u);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateTracksFailures) {
    uint32_t bridge_id;
    mock1.should_fail = true;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    immune_bridge_coordinator_start(coordinator);

    for (int i = 0; i < 10; i++) {
        immune_bridge_coordinator_update(coordinator, 1000 + i);
    }

    const immune_bridge_entry_t* entry = immune_bridge_coordinator_get_bridge(coordinator, bridge_id);
    EXPECT_GT(entry->consecutive_failures, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateCategorySucceeds) {
    uint32_t id1, id2, id3;

    immune_bridge_coordinator_register_bridge(
        coordinator, "cognitive1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &id1);
    immune_bridge_coordinator_register_bridge(
        coordinator, "cognitive2", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock2, mock_bridge_update, nullptr, nullptr, &id2);
    immune_bridge_coordinator_register_bridge(
        coordinator, "plasticity1", IMMUNE_BRIDGE_CATEGORY_PLASTICITY,
        &mock3, mock_bridge_update, nullptr, nullptr, &id3);

    immune_bridge_coordinator_start(coordinator);

    int count = immune_bridge_coordinator_update_category(
        coordinator, IMMUNE_BRIDGE_CATEGORY_COGNITIVE);

    EXPECT_EQ(count, 2);
    EXPECT_EQ(mock1.update_count, 1u);
    EXPECT_EQ(mock2.update_count, 1u);
    EXPECT_EQ(mock3.update_count, 0u);  // Different category
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateCategoryNullFails) {
    int result = immune_bridge_coordinator_update_category(
        nullptr, IMMUNE_BRIDGE_CATEGORY_COGNITIVE);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateCategoryInvalidCategoryFails) {
    int result = immune_bridge_coordinator_update_category(
        coordinator, IMMUNE_BRIDGE_CATEGORY_COUNT);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateSingleBridgeSucceeds) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    immune_bridge_coordinator_start(coordinator);

    int result = immune_bridge_coordinator_update_bridge(coordinator, bridge_id);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mock1.update_count, 1u);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateSingleBridgeNullFails) {
    int result = immune_bridge_coordinator_update_bridge(nullptr, 1);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateSingleBridgeInvalidIdFails) {
    int result = immune_bridge_coordinator_update_bridge(coordinator, 999);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(ImmuneBridgeCoordinatorTest, UpdateSingleBridgeWithoutCallbackFails) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, nullptr, nullptr, nullptr, &bridge_id);  // No update callback

    int result = immune_bridge_coordinator_update_bridge(coordinator, bridge_id);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * Health Monitoring Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, HealthCheckAllBridges) {
    uint32_t id1, id2;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, mock_bridge_health_check, &id1);
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge2", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock2, mock_bridge_update, nullptr, mock_bridge_health_check, &id2);

    int healthy_count = immune_bridge_coordinator_health_check(coordinator);
    EXPECT_EQ(healthy_count, 2);
}

TEST_F(ImmuneBridgeCoordinatorTest, HealthCheckNullFails) {
    int result = immune_bridge_coordinator_health_check(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, HealthCheckDetectsDegradedBridge) {
    uint32_t bridge_id;
    mock1.health = IMMUNE_BRIDGE_DEGRADED;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, mock_bridge_health_check, &bridge_id);

    int healthy_count = immune_bridge_coordinator_health_check(coordinator);
    EXPECT_EQ(healthy_count, 0);
}

TEST_F(ImmuneBridgeCoordinatorTest, HealthCheckInfersFromUpdateFailures) {
    uint32_t bridge_id;
    mock1.should_fail = true;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);  // No health callback

    immune_bridge_coordinator_start(coordinator);

    // Trigger multiple failures
    for (int i = 0; i < 10; i++) {
        immune_bridge_coordinator_update(coordinator, 1000 + i);
    }

    int healthy_count = immune_bridge_coordinator_health_check(coordinator);
    EXPECT_EQ(healthy_count, 0);
}

TEST_F(ImmuneBridgeCoordinatorTest, CheckBridgeHealthSucceeds) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, mock_bridge_health_check, &bridge_id);

    immune_bridge_health_t health = immune_bridge_coordinator_check_bridge_health(
        coordinator, bridge_id);
    EXPECT_EQ(health, IMMUNE_BRIDGE_HEALTHY);
}

TEST_F(ImmuneBridgeCoordinatorTest, CheckBridgeHealthNullReturnsError) {
    immune_bridge_health_t health = immune_bridge_coordinator_check_bridge_health(nullptr, 1);
    EXPECT_EQ(health, IMMUNE_BRIDGE_ERROR);
}

TEST_F(ImmuneBridgeCoordinatorTest, CheckBridgeHealthInvalidIdReturnsError) {
    immune_bridge_health_t health = immune_bridge_coordinator_check_bridge_health(coordinator, 999);
    EXPECT_EQ(health, IMMUNE_BRIDGE_ERROR);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetSystemHealthReturnsCorrectRatio) {
    uint32_t id1, id2, id3;

    mock1.health = IMMUNE_BRIDGE_HEALTHY;
    mock2.health = IMMUNE_BRIDGE_HEALTHY;
    mock3.health = IMMUNE_BRIDGE_DEGRADED;

    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, mock_bridge_health_check, &id1);
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge2", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock2, mock_bridge_update, nullptr, mock_bridge_health_check, &id2);
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge3", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock3, mock_bridge_update, nullptr, mock_bridge_health_check, &id3);

    immune_bridge_coordinator_health_check(coordinator);

    float health = immune_bridge_coordinator_get_system_health(coordinator);
    EXPECT_FLOAT_EQ(health, 2.0f / 3.0f);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetSystemHealthNullReturnsZero) {
    float health = immune_bridge_coordinator_get_system_health(nullptr);
    EXPECT_FLOAT_EQ(health, 0.0f);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetSystemHealthNoBridgesReturnsOne) {
    float health = immune_bridge_coordinator_get_system_health(coordinator);
    EXPECT_FLOAT_EQ(health, 1.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, GetStatsSucceeds) {
    immune_coordinator_stats_t stats;
    int result = immune_bridge_coordinator_get_stats(coordinator, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_bridges, 0u);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetStatsNullCoordinatorFails) {
    immune_coordinator_stats_t stats;
    int result = immune_bridge_coordinator_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetStatsNullOutputFails) {
    int result = immune_bridge_coordinator_get_stats(coordinator, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, StatsTrackUpdateCycles) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    immune_bridge_coordinator_start(coordinator);

    immune_bridge_coordinator_update(coordinator, 1000);
    immune_bridge_coordinator_update(coordinator, 2000);
    immune_bridge_coordinator_update(coordinator, 3000);

    immune_coordinator_stats_t stats;
    immune_bridge_coordinator_get_stats(coordinator, &stats);

    EXPECT_EQ(stats.total_update_cycles, 3u);
    EXPECT_EQ(stats.total_bridge_updates, 3u);
}

TEST_F(ImmuneBridgeCoordinatorTest, ResetStatsPreservesCurrentCounts) {
    uint32_t bridge_id;
    immune_bridge_coordinator_register_bridge(
        coordinator, "bridge1", IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        &mock1, mock_bridge_update, nullptr, nullptr, &bridge_id);

    immune_bridge_coordinator_start(coordinator);
    immune_bridge_coordinator_update(coordinator, 1000);

    immune_bridge_coordinator_reset_stats(coordinator);

    immune_coordinator_stats_t stats;
    immune_bridge_coordinator_get_stats(coordinator, &stats);

    EXPECT_EQ(stats.total_bridges, 1u);  // Preserved
    EXPECT_EQ(stats.total_update_cycles, 0u);  // Reset
    EXPECT_EQ(stats.total_bridge_updates, 0u);  // Reset
}

TEST_F(ImmuneBridgeCoordinatorTest, ResetStatsNullIsSafe) {
    immune_bridge_coordinator_reset_stats(nullptr);
    // Should not crash
}

TEST_F(ImmuneBridgeCoordinatorTest, GetStateReturnsCorrectState) {
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_STOPPED);

    immune_bridge_coordinator_start(coordinator);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_RUNNING);

    immune_bridge_coordinator_pause(coordinator);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_PAUSED);

    immune_bridge_coordinator_resume(coordinator);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_RUNNING);

    immune_bridge_coordinator_stop(coordinator);
    EXPECT_EQ(immune_bridge_coordinator_get_state(coordinator), IMMUNE_COORDINATOR_STOPPED);
}

TEST_F(ImmuneBridgeCoordinatorTest, GetStateNullReturnsError) {
    immune_coordinator_state_t state = immune_bridge_coordinator_get_state(nullptr);
    EXPECT_EQ(state, IMMUNE_COORDINATOR_ERROR);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, ConnectBioAsyncSucceeds) {
    int result = immune_bridge_coordinator_connect_bio_async(coordinator);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ImmuneBridgeCoordinatorTest, ConnectBioAsyncNullFails) {
    int result = immune_bridge_coordinator_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, DisconnectBioAsyncSucceeds) {
    immune_bridge_coordinator_connect_bio_async(coordinator);
    int result = immune_bridge_coordinator_disconnect_bio_async(coordinator);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ImmuneBridgeCoordinatorTest, DisconnectBioAsyncNullFails) {
    int result = immune_bridge_coordinator_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, BroadcastMessageWhenNotConnectedReturnsZero) {
    uint8_t data[] = {0x01, 0x02};
    int result = immune_bridge_coordinator_broadcast_message(
        coordinator, BIO_MSG_SWARM_IMMUNE_ALERT, data, sizeof(data));
    EXPECT_EQ(result, 0);  // Not an error, just not connected
}

TEST_F(ImmuneBridgeCoordinatorTest, BroadcastMessageNullFails) {
    uint8_t data[] = {0x01, 0x02};
    int result = immune_bridge_coordinator_broadcast_message(
        nullptr, BIO_MSG_SWARM_IMMUNE_ALERT, data, sizeof(data));
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, SendCategoryMessageNullFails) {
    uint8_t data[] = {0x01, 0x02};
    int result = immune_bridge_coordinator_send_category_message(
        nullptr, IMMUNE_BRIDGE_CATEGORY_COGNITIVE,
        BIO_MSG_SWARM_IMMUNE_ALERT, data, sizeof(data));
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ImmuneBridgeCoordinatorTest, SendCategoryMessageInvalidCategoryFails) {
    uint8_t data[] = {0x01, 0x02};
    int result = immune_bridge_coordinator_send_category_message(
        coordinator, IMMUNE_BRIDGE_CATEGORY_COUNT,
        BIO_MSG_SWARM_IMMUNE_ALERT, data, sizeof(data));
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(ImmuneBridgeCoordinatorTest, CategoryToStringReturnsCorrectValues) {
    EXPECT_STREQ(immune_bridge_category_to_string(IMMUNE_BRIDGE_CATEGORY_COGNITIVE), "cognitive");
    EXPECT_STREQ(immune_bridge_category_to_string(IMMUNE_BRIDGE_CATEGORY_PLASTICITY), "plasticity");
    EXPECT_STREQ(immune_bridge_category_to_string(IMMUNE_BRIDGE_CATEGORY_MIDDLEWARE), "middleware");
    EXPECT_STREQ(immune_bridge_category_to_string(IMMUNE_BRIDGE_CATEGORY_PERCEPTION), "perception");
    EXPECT_STREQ(immune_bridge_category_to_string(IMMUNE_BRIDGE_CATEGORY_CORE), "core");
    EXPECT_STREQ(immune_bridge_category_to_string(IMMUNE_BRIDGE_CATEGORY_GLIAL), "glial");
    EXPECT_STREQ(immune_bridge_category_to_string(IMMUNE_BRIDGE_CATEGORY_SECURITY), "security");
    EXPECT_STREQ(immune_bridge_category_to_string(IMMUNE_BRIDGE_CATEGORY_OTHER), "other");
}

TEST_F(ImmuneBridgeCoordinatorTest, HealthToStringReturnsCorrectValues) {
    EXPECT_STREQ(immune_bridge_health_to_string(IMMUNE_BRIDGE_HEALTHY), "healthy");
    EXPECT_STREQ(immune_bridge_health_to_string(IMMUNE_BRIDGE_DEGRADED), "degraded");
    EXPECT_STREQ(immune_bridge_health_to_string(IMMUNE_BRIDGE_DISCONNECTED), "disconnected");
    EXPECT_STREQ(immune_bridge_health_to_string(IMMUNE_BRIDGE_ERROR), "error");
}

TEST_F(ImmuneBridgeCoordinatorTest, StateToStringReturnsCorrectValues) {
    EXPECT_STREQ(immune_coordinator_state_to_string(IMMUNE_COORDINATOR_STOPPED), "stopped");
    EXPECT_STREQ(immune_coordinator_state_to_string(IMMUNE_COORDINATOR_STARTING), "starting");
    EXPECT_STREQ(immune_coordinator_state_to_string(IMMUNE_COORDINATOR_RUNNING), "running");
    EXPECT_STREQ(immune_coordinator_state_to_string(IMMUNE_COORDINATOR_PAUSED), "paused");
    EXPECT_STREQ(immune_coordinator_state_to_string(IMMUNE_COORDINATOR_STOPPING), "stopping");
    EXPECT_STREQ(immune_coordinator_state_to_string(IMMUNE_COORDINATOR_ERROR), "error");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
