/**
 * @file test_distributed_cognition_refactored.cpp
 * @brief Unit tests for distributed cognition async API
 *
 * Tests the async versions of distributed cognition operations:
 * - distrib_cognition_broadcast_neuromod_async
 * - distrib_cognition_propagate_calcium_wave_async
 * - distrib_cognition_coordinate_pruning_async
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <thread>

// Headers have their own extern "C" guards
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "async/nimcp_future.h"
#include "utils/error/nimcp_error_codes.h"

/**
 * @brief Test fixture for distributed cognition async tests
 */
class DistributedCognitionAsyncTest : public ::testing::Test {
protected:
    distrib_cognition_t dc;
    distrib_cognition_config_t config;

    void SetUp() override {
        // Initialize async subsystem
        nimcp_future_init(nullptr, nullptr);

        // Create config with sync disabled (for unit testing)
        memset(&config, 0, sizeof(config));
        config.enable_neuromod_sync = false;  // Disable background threads
        config.enable_glial_sync = false;
        config.enable_region_sync = false;
        config.sync_mode = SYNC_MODE_DISABLED;
        config.max_message_queue = 100;
        config.neuromod_broadcast_interval_ms = 100;
        config.neuromod_diffusion_rate = 0.1f;

        // Note: We need a mock P2P node for full testing
        // For now, we test NULL handling
        dc = nullptr;
    }

    void TearDown() override {
        if (dc) {
            distrib_cognition_destroy(dc);
            dc = nullptr;
        }
        nimcp_future_shutdown();
    }
};

//=============================================================================
// Async Neuromod Broadcast Tests
//=============================================================================

TEST_F(DistributedCognitionAsyncTest, BroadcastNeuromodAsyncNullCoordinator) {
    // Should return NULL for invalid coordinator
    nimcp_future_t future = distrib_cognition_broadcast_neuromod_async(
        nullptr, NEUROMOD_DOPAMINE, 0.5f);
    EXPECT_EQ(future, nullptr);
}

TEST_F(DistributedCognitionAsyncTest, BroadcastNeuromodAsyncInvalidConcentration) {
    // Test with invalid concentration values (even with null dc)
    nimcp_future_t future1 = distrib_cognition_broadcast_neuromod_async(
        nullptr, NEUROMOD_DOPAMINE, -0.1f);
    EXPECT_EQ(future1, nullptr);

    nimcp_future_t future2 = distrib_cognition_broadcast_neuromod_async(
        nullptr, NEUROMOD_DOPAMINE, 1.5f);
    EXPECT_EQ(future2, nullptr);
}

//=============================================================================
// Async Calcium Wave Tests
//=============================================================================

TEST_F(DistributedCognitionAsyncTest, PropagateCalciumWaveAsyncNullCoordinator) {
    // Should return NULL for invalid coordinator
    nimcp_future_t future = distrib_cognition_propagate_calcium_wave_async(
        nullptr, 1, 0.5f, 100.0f);
    EXPECT_EQ(future, nullptr);
}

TEST_F(DistributedCognitionAsyncTest, PropagateCalciumWaveAsyncInvalidLevel) {
    // Test with invalid calcium level values
    nimcp_future_t future1 = distrib_cognition_propagate_calcium_wave_async(
        nullptr, 1, -0.1f, 100.0f);
    EXPECT_EQ(future1, nullptr);

    nimcp_future_t future2 = distrib_cognition_propagate_calcium_wave_async(
        nullptr, 1, 1.5f, 100.0f);
    EXPECT_EQ(future2, nullptr);
}

//=============================================================================
// Async Pruning Coordination Tests
//=============================================================================

TEST_F(DistributedCognitionAsyncTest, CoordinatePruningAsyncNullCoordinator) {
    // Should return NULL for invalid coordinator
    nimcp_future_t future = distrib_cognition_coordinate_pruning_async(
        nullptr, 1, 2, 0.5f, 0);
    EXPECT_EQ(future, nullptr);
}

TEST_F(DistributedCognitionAsyncTest, CoordinatePruningAsyncInvalidScore) {
    // Test with invalid activity score values
    nimcp_future_t future1 = distrib_cognition_coordinate_pruning_async(
        nullptr, 1, 2, -0.1f, 0);
    EXPECT_EQ(future1, nullptr);

    nimcp_future_t future2 = distrib_cognition_coordinate_pruning_async(
        nullptr, 1, 2, 1.5f, 0);
    EXPECT_EQ(future2, nullptr);
}

TEST_F(DistributedCognitionAsyncTest, CoordinatePruningAsyncInvalidAction) {
    // Test with invalid action value (should be 0-2)
    nimcp_future_t future = distrib_cognition_coordinate_pruning_async(
        nullptr, 1, 2, 0.5f, 5);  // Invalid action
    EXPECT_EQ(future, nullptr);
}

//=============================================================================
// Synchronous API Tests (Base Operations)
//=============================================================================

TEST_F(DistributedCognitionAsyncTest, SyncBroadcastNeuromodNullCoordinator) {
    bool result = distrib_cognition_broadcast_neuromod(nullptr, NEUROMOD_DOPAMINE, 0.5f);
    EXPECT_FALSE(result);
}

TEST_F(DistributedCognitionAsyncTest, SyncBroadcastNeuromodInvalidConcentration) {
    // Even with null dc, should reject invalid concentration
    bool result1 = distrib_cognition_broadcast_neuromod(nullptr, NEUROMOD_DOPAMINE, -0.1f);
    EXPECT_FALSE(result1);

    bool result2 = distrib_cognition_broadcast_neuromod(nullptr, NEUROMOD_DOPAMINE, 1.5f);
    EXPECT_FALSE(result2);
}

TEST_F(DistributedCognitionAsyncTest, SyncCoordinatePruningNullCoordinator) {
    bool result = distrib_cognition_coordinate_pruning(nullptr, 1, 2, 0.5f, 0);
    EXPECT_FALSE(result);
}

TEST_F(DistributedCognitionAsyncTest, SyncPropagateCalciumWaveNullCoordinator) {
    bool result = distrib_cognition_propagate_calcium_wave(nullptr, 1, 0.5f, 100.0f);
    EXPECT_FALSE(result);
}

//=============================================================================
// Control API Tests
//=============================================================================

TEST_F(DistributedCognitionAsyncTest, SetSyncModeNullCoordinator) {
    bool result = distrib_cognition_set_sync_mode(nullptr, SYNC_MODE_BIDIRECTIONAL);
    EXPECT_FALSE(result);
}

TEST_F(DistributedCognitionAsyncTest, GetStatsNullCoordinator) {
    distrib_cognition_stats_t stats;
    bool result = distrib_cognition_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(DistributedCognitionAsyncTest, StartStopNullCoordinator) {
    bool start_result = distrib_cognition_start(nullptr);
    EXPECT_FALSE(start_result);

    bool stop_result = distrib_cognition_stop(nullptr);
    EXPECT_FALSE(stop_result);
}

TEST_F(DistributedCognitionAsyncTest, DestroyNullCoordinator) {
    // Should not crash with NULL
    distrib_cognition_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(DistributedCognitionAsyncTest, RegisterNeuromodPoolNullCoordinator) {
    neuromodulator_pool_t pool;
    memset(&pool, 0, sizeof(pool));
    bool result = distrib_cognition_register_neuromod_pool(nullptr, &pool);
    EXPECT_FALSE(result);
}

TEST_F(DistributedCognitionAsyncTest, RegisterNeuromodPoolNullPool) {
    // Even with null dc, should fail gracefully
    bool result = distrib_cognition_register_neuromod_pool(dc, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(DistributedCognitionAsyncTest, RegisterGlialSystemNullCoordinator) {
    glial_integration_t glial;
    memset(&glial, 0, sizeof(glial));
    bool result = distrib_cognition_register_glial_system(nullptr, &glial);
    EXPECT_FALSE(result);
}

TEST_F(DistributedCognitionAsyncTest, RegisterBrainRegionNullCoordinator) {
    brain_region_t region;
    memset(&region, 0, sizeof(region));
    bool result = distrib_cognition_register_brain_region(nullptr, &region);
    EXPECT_FALSE(result);
}

//=============================================================================
// Region Activity Broadcast Tests
//=============================================================================

TEST_F(DistributedCognitionAsyncTest, BroadcastRegionActivityNullCoordinator) {
    bool result = distrib_cognition_broadcast_region_activity(
        nullptr, 1, 0.5f, 10.0f, 50, 100);
    EXPECT_FALSE(result);
}

TEST_F(DistributedCognitionAsyncTest, BroadcastRegionActivityInvalidActivity) {
    // Test with invalid average activity values
    bool result1 = distrib_cognition_broadcast_region_activity(
        nullptr, 1, -0.1f, 10.0f, 50, 100);
    EXPECT_FALSE(result1);

    bool result2 = distrib_cognition_broadcast_region_activity(
        nullptr, 1, 1.5f, 10.0f, 50, 100);
    EXPECT_FALSE(result2);
}

TEST_F(DistributedCognitionAsyncTest, BroadcastRegionActivityInvalidNeuronCount) {
    // Test with active_neurons > total_neurons
    bool result = distrib_cognition_broadcast_region_activity(
        nullptr, 1, 0.5f, 10.0f, 150, 100);  // More active than total
    EXPECT_FALSE(result);
}
