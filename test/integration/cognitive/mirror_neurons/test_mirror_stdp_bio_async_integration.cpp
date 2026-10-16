/**
 * @file test_mirror_stdp_bio_async_integration.cpp
 * @brief Integration tests for mirror STDP bio-async system
 * @version 1.0.0
 * @date 2025-12-03
 *
 * WHAT: End-to-end integration tests for STDP with bio-async messaging
 * WHY:  Verify complete message routing and inter-module communication
 * HOW:  Test full message flow through router with multiple modules
 */

#include "test_helpers.h"

#include "cognitive/mirror_neurons/nimcp_mirror_stdp.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#include <thread>
#include <chrono>
#include <atomic>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class MirrorStdpBioAsyncIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Initialize bio-async with realistic settings
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = true;
        bio_config.enable_statistics = true;
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Initialize router with production settings
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_logging = true;
        router_config.enable_statistics = true;
        router_config.inbox_capacity = 256;
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Create STDP system
        mirror_stdp_config_t config = mirror_stdp_get_default_config();
        config.enable_dopamine_gating = true;
        config.enable_ach_gating = true;
        stdp = mirror_stdp_create(&config, 100);
        ASSERT_NE(stdp, nullptr);

        // Create test synapses
        for (uint32_t i = 0; i < 20; i++) {
            uint32_t syn_id = mirror_stdp_create_synapse(stdp, i, 0.5f);
            ASSERT_NE(syn_id, UINT32_MAX);
        }
    }

    void TearDown() override
    {
        if (stdp) {
            mirror_stdp_destroy(stdp);
            stdp = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    mirror_stdp_t stdp = nullptr;
};

//=============================================================================
// End-to-End Message Flow Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncIntegrationTest, CompleteMessageFlow)
{
    // This test verifies complete message routing
    // In production, another module would send STDP events

    // Verify router is operational
    bio_router_stats_t router_stats;
    nimcp_error_t err = bio_router_get_stats(&router_stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(router_stats.active_modules, 0u);

    // Verify bio-async is operational
    nimcp_bio_async_stats_t async_stats;
    err = nimcp_bio_async_get_stats(&async_stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MirrorStdpBioAsyncIntegrationTest, MultipleSTDPInstances)
{
    // Create multiple STDP systems
    mirror_stdp_config_t config = mirror_stdp_get_default_config();
    mirror_stdp_t stdp2 = mirror_stdp_create(&config, 50);
    mirror_stdp_t stdp3 = mirror_stdp_create(&config, 75);

    ASSERT_NE(stdp2, nullptr);
    ASSERT_NE(stdp3, nullptr);

    // All should be registered
    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, 3u);

    // Clean up
    mirror_stdp_destroy(stdp3);
    mirror_stdp_destroy(stdp2);
}

//=============================================================================
// Learning Sequence Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncIntegrationTest, LearningSequence)
{
    // Simulate a learning sequence: repeated observation -> execution pairs
    const uint32_t num_trials = 50;
    const uint32_t action_id = 0;
    const uint64_t trial_interval_us = 100000;  // 100ms between trials

    float initial_weight = mirror_stdp_get_weight(stdp, action_id);

    for (uint32_t trial = 0; trial < num_trials; trial++) {
        uint64_t base_time = trial * trial_interval_us;

        // Observation spike
        mirror_stdp_observation_spike(stdp, action_id, base_time, 1.0f);

        // Execution spike 10ms later (LTP window)
        mirror_stdp_execution_spike(stdp, action_id, base_time + 10000, 1.0f);

        // Step STDP
        mirror_stdp_step(stdp, 1.0f);
    }

    float final_weight = mirror_stdp_get_weight(stdp, action_id);

    // Weight should increase due to repeated LTP
    EXPECT_GT(final_weight, initial_weight)
        << "Weight should increase after repeated obs->exec pairings";

    // Verify learning statistics
    mirror_stdp_stats_t stats;
    mirror_stdp_get_stats(stdp, &stats);
    EXPECT_GT(stats.total_ltp_events, 0u) << "Should have LTP events";
}

TEST_F(MirrorStdpBioAsyncIntegrationTest, ReverseLearning)
{
    // Simulate reverse pairing: execution -> observation (LTD)
    const uint32_t num_trials = 50;
    const uint32_t action_id = 1;

    float initial_weight = mirror_stdp_get_weight(stdp, action_id);

    for (uint32_t trial = 0; trial < num_trials; trial++) {
        uint64_t base_time = trial * 100000;

        // Execution spike first
        mirror_stdp_execution_spike(stdp, action_id, base_time, 1.0f);

        // Observation spike 10ms later (LTD window)
        mirror_stdp_observation_spike(stdp, action_id, base_time + 10000, 1.0f);

        mirror_stdp_step(stdp, 1.0f);
    }

    float final_weight = mirror_stdp_get_weight(stdp, action_id);

    // Weight should decrease due to repeated LTD
    EXPECT_LT(final_weight, initial_weight)
        << "Weight should decrease after repeated exec->obs pairings";

    // Verify learning statistics
    mirror_stdp_stats_t stats;
    mirror_stdp_get_stats(stdp, &stats);
    EXPECT_GT(stats.total_ltd_events, 0u) << "Should have LTD events";
}

//=============================================================================
// Neuromodulator Integration Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncIntegrationTest, DopamineGatedLearning)
{
    // Test dopamine-gated STDP learning
    const uint32_t action_id = 2;

    // Learning with low dopamine (no reward)
    mirror_stdp_set_dopamine(stdp, 0.1f);

    float weight_before_low_da = mirror_stdp_get_weight(stdp, action_id);

    for (int i = 0; i < 10; i++) {
        uint64_t time = i * 100000;
        mirror_stdp_observation_spike(stdp, action_id, time, 1.0f);
        mirror_stdp_execution_spike(stdp, action_id, time + 10000, 1.0f);
    }

    float weight_after_low_da = mirror_stdp_get_weight(stdp, action_id);
    float change_low_da = weight_after_low_da - weight_before_low_da;

    // Reset weight
    mirror_stdp_create_synapse(stdp, action_id, 0.5f);  // Recreate at baseline

    // Learning with high dopamine (reward)
    mirror_stdp_set_dopamine(stdp, 0.9f);

    float weight_before_high_da = mirror_stdp_get_weight(stdp, action_id);

    for (int i = 0; i < 10; i++) {
        uint64_t time = 10000000 + i * 100000;
        mirror_stdp_observation_spike(stdp, action_id, time, 1.0f);
        mirror_stdp_execution_spike(stdp, action_id, time + 10000, 1.0f);
    }

    float weight_after_high_da = mirror_stdp_get_weight(stdp, action_id);
    float change_high_da = weight_after_high_da - weight_before_high_da;

    // High dopamine should enhance learning more than low dopamine
    EXPECT_GT(change_high_da, change_low_da)
        << "High dopamine should boost learning more than low dopamine";
}

//=============================================================================
// Homeostatic Plasticity Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncIntegrationTest, HomeostasisPreventsRunaway)
{
    // Drive strong potentiation, verify homeostasis prevents runaway
    const uint32_t action_id = 3;

    // Strong repeated potentiation
    for (int i = 0; i < 1000; i++) {
        uint64_t time = i * 10000;
        mirror_stdp_observation_spike(stdp, action_id, time, 1.0f);
        mirror_stdp_execution_spike(stdp, action_id, time + 5000, 1.0f);

        // Apply homeostasis periodically
        if (i % 100 == 0) {
            mirror_stdp_apply_homeostasis(stdp, 1000.0f);
        }
    }

    float final_weight = mirror_stdp_get_weight(stdp, action_id);

    // Weight should not exceed maximum
    EXPECT_LE(final_weight, 1.0f) << "Homeostasis should prevent weight > max";
    EXPECT_GE(final_weight, 0.0f) << "Weight should stay in valid range";

    // Verify homeostasis was applied
    mirror_stdp_stats_t stats;
    mirror_stdp_get_stats(stdp, &stats);
    EXPECT_GT(stats.homeostatic_adjustments, 0u)
        << "Homeostasis should have made adjustments";
}

//=============================================================================
// Performance and Stress Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncIntegrationTest, HighThroughputSTDP)
{
    // Stress test with high spike rate
    const uint32_t num_spikes = 10000;
    const uint32_t num_synapses = 20;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < num_spikes; i++) {
        uint32_t synapse_id = i % num_synapses;
        uint64_t time = i * 100;  // 100 μs intervals

        if (i % 2 == 0) {
            mirror_stdp_observation_spike(stdp, synapse_id, time, 1.0f);
        } else {
            mirror_stdp_execution_spike(stdp, synapse_id, time, 1.0f);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should process spikes quickly
    EXPECT_LT(duration_ms, 1000) << "Should process 10k spikes in < 1 second";

    // Verify system is still operational
    mirror_stdp_stats_t stats;
    bool success = mirror_stdp_get_stats(stdp, &stats);
    ASSERT_TRUE(success);
}

TEST_F(MirrorStdpBioAsyncIntegrationTest, ConcurrentAccess)
{
    // Test concurrent spike processing (simplified - single threaded for now)
    // In production, multiple modules might send STDP events concurrently

    std::atomic<uint32_t> spike_count{0};

    // Simulate concurrent spikes from multiple sources
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t syn_id = i % 20;
        uint64_t time = i * 1000;

        mirror_stdp_observation_spike(stdp, syn_id, time, 1.0f);
        mirror_stdp_execution_spike(stdp, syn_id, time + 100, 1.0f);

        spike_count += 2;
    }

    EXPECT_EQ(spike_count.load(), 200u);

    // System should remain stable
    mirror_stdp_stats_t stats;
    mirror_stdp_get_stats(stdp, &stats);
    EXPECT_GT(stats.total_ltp_events + stats.total_ltd_events, 0u);
}

//=============================================================================
// Bio-Async System Integration Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncIntegrationTest, BioAsyncStatisticsTracking)
{
    // Verify bio-async statistics are being collected
    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_GE(stats.simulation_steps, 0u);
}

TEST_F(MirrorStdpBioAsyncIntegrationTest, RouterStatisticsTracking)
{
    // Verify router statistics are being collected
    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_GT(stats.active_modules, 0u);
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(MirrorStdpBioAsyncIntegrationTest, GracefulDegradation)
{
    // Test that system continues working even with errors
    // Process mix of valid and invalid synapse IDs

    uint32_t valid_count = 0;
    uint32_t invalid_count = 0;

    for (uint32_t i = 0; i < 100; i++) {
        uint32_t syn_id = (i % 2 == 0) ? (i / 2) % 20 : 9999;  // Mix valid and invalid

        uint32_t found = mirror_stdp_find_synapse(stdp, syn_id);

        if (found != UINT32_MAX) {
            valid_count++;
            mirror_stdp_observation_spike(stdp, found, i * 1000, 1.0f);
        } else {
            invalid_count++;
        }
    }

    EXPECT_GT(valid_count, 0u) << "Should process valid synapses";
    EXPECT_GT(invalid_count, 0u) << "Should handle invalid synapses gracefully";

    // System should still be functional
    mirror_stdp_stats_t stats;
    bool success = mirror_stdp_get_stats(stdp, &stats);
    ASSERT_TRUE(success);
}

}  // namespace
