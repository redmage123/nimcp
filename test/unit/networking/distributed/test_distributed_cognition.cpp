/**
 * @file test_distributed_cognition.cpp
 * @brief TDD test suite for distributed cognitive integration
 *
 * Tests cover:
 * - Creation/destruction lifecycle
 * - Configuration and defaults
 * - Registration API (neuromod/glial/region)
 * - Broadcasting functions with validation
 * - Worker thread lifecycle
 * - Statistics tracking
 * - Thread safety
 * - Error handling
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "networking/p2p/nimcp_p2pnode.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/memory/nimcp_memory.h"
#include <unistd.h>

//=============================================================================
// Test Fixtures
//=============================================================================

class DistributedCognitionTest : public ::testing::Test {
protected:
    p2p_node_t p2p_node;
    distrib_cognition_t dc;

    void SetUp() override {
        // Create a minimal P2P node for testing
        // NOTE: This is a mock - real P2P integration will be tested separately
        p2p_node = (p2p_node_t)0x1234; // Mock pointer
        dc = nullptr;
    }

    void TearDown() override {
        if (dc) {
            distrib_cognition_destroy(dc);
            dc = nullptr;
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(DistributedCognitionTest, CreateWithDefaultConfig) {
    dc = distrib_cognition_create(nullptr, p2p_node);

    ASSERT_NE(dc, nullptr) << "Should create coordinator with default config";

    // Verify default configuration is applied
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
    EXPECT_EQ(stats.peers_connected, 0);
}

TEST_F(DistributedCognitionTest, CreateWithCustomConfig) {
    distrib_cognition_config_t config;
    config.enable_neuromod_sync = true;
    config.neuromod_broadcast_interval_ms = 200;  // 5 Hz
    config.neuromod_diffusion_rate = 0.2f;        // 20%
    config.enable_glial_sync = false;             // Disabled
    config.glial_sync_interval_ms = 1000;
    config.enable_region_sync = true;
    config.region_sync_interval_ms = 100;
    config.sync_mode = SYNC_MODE_PUSH;
    config.max_message_queue = 500;

    dc = distrib_cognition_create(&config, p2p_node);

    ASSERT_NE(dc, nullptr) << "Should create coordinator with custom config";
}

TEST_F(DistributedCognitionTest, CreateWithNullP2PNode) {
    dc = distrib_cognition_create(nullptr, nullptr);

    EXPECT_EQ(dc, nullptr) << "Should reject NULL P2P node";
}

TEST_F(DistributedCognitionTest, DestroyNullSafe) {
    distrib_cognition_destroy(nullptr);
    // Should not crash
}

TEST_F(DistributedCognitionTest, DestroyAfterStart) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Start and immediately destroy - should cleanup threads
    ASSERT_TRUE(distrib_cognition_start(dc));
    usleep(50000); // 50ms to let threads start

    distrib_cognition_destroy(dc);
    dc = nullptr; // Prevent double-free in TearDown

    // If we get here without hanging, thread cleanup worked
}

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(DistributedCognitionTest, RegisterNeuromodPool) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    neuromodulator_pool_t pool;  // Mock pool

    EXPECT_TRUE(distrib_cognition_register_neuromod_pool(dc, &pool));

    // Register multiple pools
    neuromodulator_pool_t pool2;
    EXPECT_TRUE(distrib_cognition_register_neuromod_pool(dc, &pool2));
}

TEST_F(DistributedCognitionTest, RegisterNeuromodPoolNullArgs) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    neuromodulator_pool_t pool;

    EXPECT_FALSE(distrib_cognition_register_neuromod_pool(nullptr, &pool));
    EXPECT_FALSE(distrib_cognition_register_neuromod_pool(dc, nullptr));
}

TEST_F(DistributedCognitionTest, RegisterGlialSystem) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    glial_integration_t glial;  // Mock glial system

    EXPECT_TRUE(distrib_cognition_register_glial_system(dc, &glial));

    // Register multiple systems
    glial_integration_t glial2;
    EXPECT_TRUE(distrib_cognition_register_glial_system(dc, &glial2));
}

TEST_F(DistributedCognitionTest, RegisterGlialSystemNullArgs) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    glial_integration_t glial;

    EXPECT_FALSE(distrib_cognition_register_glial_system(nullptr, &glial));
    EXPECT_FALSE(distrib_cognition_register_glial_system(dc, nullptr));
}

TEST_F(DistributedCognitionTest, RegisterBrainRegion) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    brain_region_t region;  // Mock brain region

    EXPECT_TRUE(distrib_cognition_register_brain_region(dc, &region));

    // Register multiple regions
    brain_region_t region2;
    EXPECT_TRUE(distrib_cognition_register_brain_region(dc, &region2));
}

TEST_F(DistributedCognitionTest, RegisterBrainRegionNullArgs) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    brain_region_t region;

    EXPECT_FALSE(distrib_cognition_register_brain_region(nullptr, &region));
    EXPECT_FALSE(distrib_cognition_register_brain_region(dc, nullptr));
}

//=============================================================================
// Broadcasting Tests
//=============================================================================

TEST_F(DistributedCognitionTest, BroadcastNeuromodulator) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Valid broadcast
    EXPECT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.75f));

    // Check statistics updated
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.neuromod_broadcasts, 1);
    EXPECT_EQ(stats.messages_sent, 1);
}

TEST_F(DistributedCognitionTest, BroadcastNeuromodulatorValidation) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Invalid concentration - too low
    EXPECT_FALSE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_SEROTONIN, -0.1f));

    // Invalid concentration - too high
    EXPECT_FALSE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_ACETYLCHOLINE, 1.5f));

    // Valid boundary values
    EXPECT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_NOREPINEPHRINE, 0.0f));
    EXPECT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_GABA, 1.0f));
}

TEST_F(DistributedCognitionTest, CoordinatePruning) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Valid pruning coordination
    EXPECT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.5f, 1));

    // Check statistics
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.glial_pruning_coordinations, 1);
}

TEST_F(DistributedCognitionTest, CoordinatePruningValidation) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Invalid activity score
    EXPECT_FALSE(distrib_cognition_coordinate_pruning(dc, 100, 200, -0.1f, 0));
    EXPECT_FALSE(distrib_cognition_coordinate_pruning(dc, 100, 200, 1.5f, 0));

    // Invalid action (valid: 0=monitor, 1=prune, 2=preserve)
    EXPECT_FALSE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.5f, 3));
    EXPECT_FALSE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.5f, 255));

    // Valid actions
    EXPECT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.5f, 0));
    EXPECT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.5f, 1));
    EXPECT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.5f, 2));
}

TEST_F(DistributedCognitionTest, PropagateCalciumWave) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Valid calcium wave
    EXPECT_TRUE(distrib_cognition_propagate_calcium_wave(dc, 42, 0.8f, 25.5f));

    // Check statistics
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.glial_calcium_propagations, 1);
}

TEST_F(DistributedCognitionTest, PropagateCalciumWaveValidation) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Invalid calcium level
    EXPECT_FALSE(distrib_cognition_propagate_calcium_wave(dc, 42, -0.1f, 25.0f));
    EXPECT_FALSE(distrib_cognition_propagate_calcium_wave(dc, 42, 1.5f, 25.0f));

    // Valid boundary values
    EXPECT_TRUE(distrib_cognition_propagate_calcium_wave(dc, 42, 0.0f, 25.0f));
    EXPECT_TRUE(distrib_cognition_propagate_calcium_wave(dc, 42, 1.0f, 25.0f));
}

TEST_F(DistributedCognitionTest, BroadcastRegionActivity) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Valid region activity broadcast
    EXPECT_TRUE(distrib_cognition_broadcast_region_activity(dc, 1, 0.6f, 15.5f, 800, 1000));

    // Check statistics
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.region_activity_broadcasts, 1);
}

TEST_F(DistributedCognitionTest, BroadcastRegionActivityValidation) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Invalid average activity
    EXPECT_FALSE(distrib_cognition_broadcast_region_activity(dc, 1, -0.1f, 15.0f, 800, 1000));
    EXPECT_FALSE(distrib_cognition_broadcast_region_activity(dc, 1, 1.5f, 15.0f, 800, 1000));

    // Active neurons exceeds total
    EXPECT_FALSE(distrib_cognition_broadcast_region_activity(dc, 1, 0.5f, 15.0f, 1200, 1000));

    // Valid boundaries
    EXPECT_TRUE(distrib_cognition_broadcast_region_activity(dc, 1, 0.0f, 0.0f, 0, 1000));
    EXPECT_TRUE(distrib_cognition_broadcast_region_activity(dc, 1, 1.0f, 100.0f, 1000, 1000));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DistributedCognitionTest, StartStop) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Start
    EXPECT_TRUE(distrib_cognition_start(dc));

    // Starting again should return true but warn
    EXPECT_TRUE(distrib_cognition_start(dc));

    usleep(100000); // 100ms to let threads do some work

    // Stop
    EXPECT_TRUE(distrib_cognition_stop(dc));

    // Stopping again should return true but warn
    EXPECT_TRUE(distrib_cognition_stop(dc));
}

TEST_F(DistributedCognitionTest, StartStopNullSafe) {
    EXPECT_FALSE(distrib_cognition_start(nullptr));
    EXPECT_FALSE(distrib_cognition_stop(nullptr));
}

TEST_F(DistributedCognitionTest, StartStopWithDisabledSync) {
    distrib_cognition_config_t config;
    config.enable_neuromod_sync = false;
    config.enable_glial_sync = false;
    config.enable_region_sync = false;
    config.neuromod_broadcast_interval_ms = 100;
    config.neuromod_diffusion_rate = 0.1f;
    config.glial_sync_interval_ms = 100;
    config.region_sync_interval_ms = 100;
    config.sync_mode = SYNC_MODE_DISABLED;
    config.max_message_queue = 1000;

    dc = distrib_cognition_create(&config, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Should start/stop even with all sync disabled
    EXPECT_TRUE(distrib_cognition_start(dc));
    usleep(50000);
    EXPECT_TRUE(distrib_cognition_stop(dc));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DistributedCognitionTest, GetStatistics) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));

    // Initial state
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
    EXPECT_EQ(stats.messages_dropped, 0);
    EXPECT_EQ(stats.peers_connected, 0);
    EXPECT_EQ(stats.neuromod_broadcasts, 0);
    EXPECT_EQ(stats.glial_pruning_coordinations, 0);
    EXPECT_EQ(stats.glial_calcium_propagations, 0);
    EXPECT_EQ(stats.region_activity_broadcasts, 0);
}

TEST_F(DistributedCognitionTest, StatisticsUpdate) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Perform various operations
    distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.5f);
    distrib_cognition_broadcast_neuromod(dc, NEUROMOD_SEROTONIN, 0.3f);
    distrib_cognition_coordinate_pruning(dc, 1, 2, 0.5f, 1);
    distrib_cognition_propagate_calcium_wave(dc, 10, 0.7f, 20.0f);
    distrib_cognition_broadcast_region_activity(dc, 1, 0.5f, 10.0f, 500, 1000);

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));

    EXPECT_EQ(stats.neuromod_broadcasts, 2);
    EXPECT_EQ(stats.glial_pruning_coordinations, 1);
    EXPECT_EQ(stats.glial_calcium_propagations, 1);
    EXPECT_EQ(stats.region_activity_broadcasts, 1);
    EXPECT_EQ(stats.messages_sent, 5); // Total messages
}

TEST_F(DistributedCognitionTest, GetStatisticsNullArgs) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    distrib_cognition_stats_t stats;

    EXPECT_FALSE(distrib_cognition_get_stats(nullptr, &stats));
    EXPECT_FALSE(distrib_cognition_get_stats(dc, nullptr));
}

//=============================================================================
// Sync Mode Tests
//=============================================================================

TEST_F(DistributedCognitionTest, SetSyncMode) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    EXPECT_TRUE(distrib_cognition_set_sync_mode(dc, SYNC_MODE_PUSH));
    EXPECT_TRUE(distrib_cognition_set_sync_mode(dc, SYNC_MODE_PULL));
    EXPECT_TRUE(distrib_cognition_set_sync_mode(dc, SYNC_MODE_BIDIRECTIONAL));
    EXPECT_TRUE(distrib_cognition_set_sync_mode(dc, SYNC_MODE_DISABLED));
}

TEST_F(DistributedCognitionTest, SetSyncModeInvalid) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Invalid sync mode
    EXPECT_FALSE(distrib_cognition_set_sync_mode(dc, static_cast<sync_mode_t>(99)));

    // NULL coordinator
    EXPECT_FALSE(distrib_cognition_set_sync_mode(nullptr, SYNC_MODE_PUSH));
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(DistributedCognitionTest, ConcurrentBroadcasts) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Start coordinator
    ASSERT_TRUE(distrib_cognition_start(dc));

    // Perform concurrent broadcasts from multiple "threads" (simulated)
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.5f));
        EXPECT_TRUE(distrib_cognition_coordinate_pruning(dc, i, i+1, 0.5f, 0));
        EXPECT_TRUE(distrib_cognition_propagate_calcium_wave(dc, i, 0.5f, 10.0f));
    }

    // Check all operations completed
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.neuromod_broadcasts, 10);
    EXPECT_EQ(stats.glial_pruning_coordinations, 10);
    EXPECT_EQ(stats.glial_calcium_propagations, 10);

    ASSERT_TRUE(distrib_cognition_stop(dc));
}

TEST_F(DistributedCognitionTest, ConcurrentRegistrations) {
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Register multiple pools while coordinator is running
    ASSERT_TRUE(distrib_cognition_start(dc));

    neuromodulator_pool_t pools[5];
    glial_integration_t glials[5];
    brain_region_t regions[5];

    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(distrib_cognition_register_neuromod_pool(dc, &pools[i]));
        EXPECT_TRUE(distrib_cognition_register_glial_system(dc, &glials[i]));
        EXPECT_TRUE(distrib_cognition_register_brain_region(dc, &regions[i]));
    }

    ASSERT_TRUE(distrib_cognition_stop(dc));
}

//=============================================================================
// Memory Tests
//=============================================================================

TEST_F(DistributedCognitionTest, NoMemoryLeaks) {
    // Get initial memory stats
    nimcp_memory_stats_t initial_stats;
    nimcp_memory_get_stats(&initial_stats);

    // Create, use, and destroy coordinator
    dc = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc, nullptr);

    neuromodulator_pool_t pool;
    distrib_cognition_register_neuromod_pool(dc, &pool);
    distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.5f);

    ASSERT_TRUE(distrib_cognition_start(dc));
    usleep(50000);
    ASSERT_TRUE(distrib_cognition_stop(dc));

    distrib_cognition_destroy(dc);
    dc = nullptr;

    // Check memory stats
    nimcp_memory_stats_t final_stats;
    nimcp_memory_get_stats(&final_stats);

    EXPECT_EQ(initial_stats.current_allocated, final_stats.current_allocated)
        << "Memory leak detected";
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(DistributedCognitionTest, FullWorkflowSimulation) {
    // Create coordinator with custom config
    distrib_cognition_config_t config;
    config.enable_neuromod_sync = true;
    config.neuromod_broadcast_interval_ms = 50;  // Fast for testing
    config.neuromod_diffusion_rate = 0.15f;
    config.enable_glial_sync = true;
    config.glial_sync_interval_ms = 50;
    config.enable_region_sync = true;
    config.region_sync_interval_ms = 50;
    config.sync_mode = SYNC_MODE_BIDIRECTIONAL;
    config.max_message_queue = 1000;

    dc = distrib_cognition_create(&config, p2p_node);
    ASSERT_NE(dc, nullptr);

    // Register systems
    neuromodulator_pool_t pool;
    glial_integration_t glial;
    brain_region_t region;

    ASSERT_TRUE(distrib_cognition_register_neuromod_pool(dc, &pool));
    ASSERT_TRUE(distrib_cognition_register_glial_system(dc, &glial));
    ASSERT_TRUE(distrib_cognition_register_brain_region(dc, &region));

    // Start coordinator
    ASSERT_TRUE(distrib_cognition_start(dc));

    // Simulate distributed activity
    for (int i = 0; i < 5; i++) {
        distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.7f);
        distrib_cognition_coordinate_pruning(dc, i*10, i*10+1, 0.3f, 1);
        distrib_cognition_propagate_calcium_wave(dc, i, 0.6f, 15.0f);
        distrib_cognition_broadcast_region_activity(dc, 1, 0.5f, 12.0f, 700, 1000);
        usleep(10000); // 10ms between operations
    }

    // Let worker threads process
    usleep(200000); // 200ms

    // Check statistics
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));

    EXPECT_EQ(stats.neuromod_broadcasts, 5);
    EXPECT_EQ(stats.glial_pruning_coordinations, 5);
    EXPECT_EQ(stats.glial_calcium_propagations, 5);
    EXPECT_EQ(stats.region_activity_broadcasts, 5);
    EXPECT_EQ(stats.messages_sent, 20);

    // Clean shutdown
    ASSERT_TRUE(distrib_cognition_stop(dc));
}
