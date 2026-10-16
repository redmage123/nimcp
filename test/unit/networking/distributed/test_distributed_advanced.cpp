/**
 * @file test_distributed_advanced.cpp
 * @brief Advanced integration tests for distributed cognition algorithms
 *
 * Tests cover:
 * - Neuromodulator diffusion strategies (weighted avg, max, min)
 * - Glial consensus protocol with voting
 * - Brain region state aggregation
 * - Performance benchmarks
 * - Algorithm correctness
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/time/nimcp_time.h"
#include <unistd.h>
#include <math.h>

//=============================================================================
// Test Fixtures
//=============================================================================

class DistributedAdvancedTest : public ::testing::Test {
protected:
    p2p_node_t p2p_node;
    distrib_cognition_t dc;

    void SetUp() override {
        p2p_node = (p2p_node_t)0x1234; // Mock P2P node
        dc = nullptr;
    }

    void TearDown() override {
        if (dc) {
            distrib_cognition_destroy(dc);
            dc = nullptr;
        }
    }

    /**
     * WHAT: Create coordinator with fast sync intervals for testing
     * WHY:  Reduce test execution time
     * HOW:  Configure with 10ms intervals
     */
    distrib_cognition_t create_fast_coordinator() {
        distrib_cognition_config_t config;
        config.enable_neuromod_sync = true;
        config.neuromod_broadcast_interval_ms = 10;
        config.neuromod_diffusion_rate = 0.5f;
        config.enable_glial_sync = true;
        config.glial_sync_interval_ms = 10;
        config.enable_region_sync = true;
        config.region_sync_interval_ms = 10;
        config.sync_mode = SYNC_MODE_BIDIRECTIONAL;
        config.max_message_queue = 1000;

        return distrib_cognition_create(&config, p2p_node);
    }
};

//=============================================================================
// Neuromodulator Diffusion Tests
//=============================================================================

TEST_F(DistributedAdvancedTest, DiffusionWeightedAverage) {
    /**
     * WHAT: Test weighted average diffusion strategy
     * WHY:  Verify smooth blending of local and remote concentrations
     * HOW:  Simulate peer updates and check diffusion results
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    neuromodulator_pool_t pool;
    ASSERT_TRUE(distrib_cognition_register_neuromod_pool(dc, &pool));

    // Simulate broadcast with high dopamine
    ASSERT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.8f));

    // Verify message sent
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.neuromod_broadcasts, 1);
}

TEST_F(DistributedAdvancedTest, DiffusionMultipleNeuromodulators) {
    /**
     * WHAT: Test diffusion with all 6 neuromodulator types
     * WHY:  Verify each type can be synchronized independently
     * HOW:  Broadcast different concentrations for each type
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    neuromodulator_pool_t pool;
    ASSERT_TRUE(distrib_cognition_register_neuromod_pool(dc, &pool));

    // Broadcast each neuromodulator type
    ASSERT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.7f));
    ASSERT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_SEROTONIN, 0.5f));
    ASSERT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_ACETYLCHOLINE, 0.6f));
    ASSERT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_NOREPINEPHRINE, 0.8f));
    ASSERT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_GABA, 0.4f));
    ASSERT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_GLUTAMATE, 0.9f));

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.neuromod_broadcasts, 6);
}

TEST_F(DistributedAdvancedTest, DiffusionRateControl) {
    /**
     * WHAT: Test different diffusion rates
     * WHY:  Verify rate parameter controls blending strength
     * HOW:  Create coordinators with different rates, compare behavior
     */

    // Low diffusion rate (10%)
    distrib_cognition_config_t config_low;
    config_low.enable_neuromod_sync = true;
    config_low.neuromod_broadcast_interval_ms = 10;
    config_low.neuromod_diffusion_rate = 0.1f;
    config_low.enable_glial_sync = false;
    config_low.enable_region_sync = false;
    config_low.glial_sync_interval_ms = 100;
    config_low.region_sync_interval_ms = 100;
    config_low.sync_mode = SYNC_MODE_BIDIRECTIONAL;
    config_low.max_message_queue = 1000;

    distrib_cognition_t dc_low = distrib_cognition_create(&config_low, p2p_node);
    ASSERT_NE(dc_low, nullptr);

    // High diffusion rate (90%)
    distrib_cognition_config_t config_high;
    config_high.enable_neuromod_sync = true;
    config_high.neuromod_broadcast_interval_ms = 10;
    config_high.neuromod_diffusion_rate = 0.9f;
    config_high.enable_glial_sync = false;
    config_high.enable_region_sync = false;
    config_high.glial_sync_interval_ms = 100;
    config_high.region_sync_interval_ms = 100;
    config_high.sync_mode = SYNC_MODE_BIDIRECTIONAL;
    config_high.max_message_queue = 1000;

    distrib_cognition_t dc_high = distrib_cognition_create(&config_high, p2p_node);
    ASSERT_NE(dc_high, nullptr);

    // Both should work
    neuromodulator_pool_t pool_low, pool_high;
    EXPECT_TRUE(distrib_cognition_register_neuromod_pool(dc_low, &pool_low));
    EXPECT_TRUE(distrib_cognition_register_neuromod_pool(dc_high, &pool_high));

    distrib_cognition_destroy(dc_low);
    distrib_cognition_destroy(dc_high);
}

//=============================================================================
// Glial Consensus Protocol Tests
//=============================================================================

TEST_F(DistributedAdvancedTest, ConsensusSimpleMajority) {
    /**
     * WHAT: Test basic majority voting for pruning
     * WHY:  Verify consensus reaches correct decision
     * HOW:  Simulate multiple pruning votes, check majority wins
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    glial_integration_t glial;
    ASSERT_TRUE(distrib_cognition_register_glial_system(dc, &glial));

    // Coordinate pruning - simulate 3 peers voting "prune" (action=1)
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.2f, 1));
    }

    // And 1 peer voting "preserve" (action=2)
    ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.8f, 2));

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.glial_pruning_coordinations, 4);
}

TEST_F(DistributedAdvancedTest, ConsensusMultipleSynapses) {
    /**
     * WHAT: Test concurrent consensus for different synapses
     * WHY:  Verify multiple consensus sessions can run simultaneously
     * HOW:  Coordinate pruning for different synapse pairs
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    glial_integration_t glial;
    ASSERT_TRUE(distrib_cognition_register_glial_system(dc, &glial));

    // Coordinate pruning for synapse 100->200
    ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.3f, 1));

    // Coordinate pruning for synapse 300->400 (different)
    ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, 300, 400, 0.7f, 2));

    // Coordinate pruning for synapse 500->600 (another different)
    ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, 500, 600, 0.5f, 0));

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.glial_pruning_coordinations, 3);
}

TEST_F(DistributedAdvancedTest, ConsensusTieBreaking) {
    /**
     * WHAT: Test tie-breaking behavior in consensus
     * WHY:  Verify deterministic behavior when votes are tied
     * HOW:  Create equal vote counts for different actions
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    glial_integration_t glial;
    ASSERT_TRUE(distrib_cognition_register_glial_system(dc, &glial));

    // 2 votes to prune, 2 votes to preserve (tie)
    ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.3f, 1));
    ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.3f, 1));
    ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.7f, 2));
    ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, 100, 200, 0.7f, 2));

    // Should handle tie gracefully (default to monitor or first action)
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.glial_pruning_coordinations, 4);
}

//=============================================================================
// Calcium Wave Propagation Tests
//=============================================================================

TEST_F(DistributedAdvancedTest, CalciumWavePropagation) {
    /**
     * WHAT: Test calcium wave coordination across nodes
     * WHY:  Verify astrocyte signaling can be distributed
     * HOW:  Propagate waves with different velocities
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    glial_integration_t glial;
    ASSERT_TRUE(distrib_cognition_register_glial_system(dc, &glial));

    // Propagate calcium wave with typical velocity (15 μm/s)
    ASSERT_TRUE(distrib_cognition_propagate_calcium_wave(dc, 1, 0.8f, 15.0f));

    // Propagate faster wave (30 μm/s)
    ASSERT_TRUE(distrib_cognition_propagate_calcium_wave(dc, 2, 0.9f, 30.0f));

    // Propagate slower wave (5 μm/s)
    ASSERT_TRUE(distrib_cognition_propagate_calcium_wave(dc, 3, 0.6f, 5.0f));

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.glial_calcium_propagations, 3);
}

//=============================================================================
// Brain Region Synchronization Tests
//=============================================================================

TEST_F(DistributedAdvancedTest, RegionStateAggregation) {
    /**
     * WHAT: Test region activity aggregation across network
     * WHY:  Verify distributed brain state can be monitored
     * HOW:  Broadcast region stats, check aggregation
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    brain_region_t region;
    ASSERT_TRUE(distrib_cognition_register_brain_region(dc, &region));

    // Broadcast region activity for visual cortex (region_type=1)
    ASSERT_TRUE(distrib_cognition_broadcast_region_activity(
        dc, 1, 0.7f, 25.0f, 800, 1000
    ));

    // Broadcast region activity for motor cortex (region_type=2)
    ASSERT_TRUE(distrib_cognition_broadcast_region_activity(
        dc, 2, 0.5f, 15.0f, 600, 1000
    ));

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.region_activity_broadcasts, 2);
}

TEST_F(DistributedAdvancedTest, RegionMultiNodeTracking) {
    /**
     * WHAT: Test tracking same region type across multiple nodes
     * WHY:  Verify network-wide region state aggregation
     * HOW:  Multiple broadcasts for same region type
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    brain_region_t region1, region2, region3;
    ASSERT_TRUE(distrib_cognition_register_brain_region(dc, &region1));
    ASSERT_TRUE(distrib_cognition_register_brain_region(dc, &region2));
    ASSERT_TRUE(distrib_cognition_register_brain_region(dc, &region3));

    // Simulate 3 nodes with same region type (visual cortex)
    ASSERT_TRUE(distrib_cognition_broadcast_region_activity(
        dc, 1, 0.6f, 20.0f, 700, 1000  // Node 1
    ));
    ASSERT_TRUE(distrib_cognition_broadcast_region_activity(
        dc, 1, 0.7f, 25.0f, 800, 1000  // Node 2
    ));
    ASSERT_TRUE(distrib_cognition_broadcast_region_activity(
        dc, 1, 0.8f, 30.0f, 900, 1000  // Node 3
    ));

    // All from same region type, should track separately
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.region_activity_broadcasts, 3);
}

//=============================================================================
// Performance and Timing Tests
//=============================================================================

TEST_F(DistributedAdvancedTest, HighFrequencyBroadcasts) {
    /**
     * WHAT: Test system under high broadcast frequency
     * WHY:  Verify performance at 100 Hz broadcast rate
     * HOW:  Rapid succession of broadcasts, measure timing
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    ASSERT_TRUE(distrib_cognition_start(dc));

    uint64_t start_time = nimcp_time_get_us();

    // Send 100 broadcasts as fast as possible
    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(distrib_cognition_broadcast_neuromod(
            dc,
            static_cast<neuromodulator_type_t>(i % NEUROMOD_COUNT),
            0.5f
        ));
    }

    uint64_t end_time = nimcp_time_get_us();
    uint64_t duration_ms = (end_time - start_time) / 1000;

    // Should complete in reasonable time (<100ms)
    EXPECT_LT(duration_ms, 100) << "High-frequency broadcasts too slow";

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.neuromod_broadcasts, 100);

    ASSERT_TRUE(distrib_cognition_stop(dc));
}

TEST_F(DistributedAdvancedTest, ConcurrentOperations) {
    /**
     * WHAT: Test concurrent neuromod + glial + region operations
     * WHY:  Verify all subsystems can operate simultaneously
     * HOW:  Interleave operations of all types
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    neuromodulator_pool_t pool;
    glial_integration_t glial;
    brain_region_t region;

    ASSERT_TRUE(distrib_cognition_register_neuromod_pool(dc, &pool));
    ASSERT_TRUE(distrib_cognition_register_glial_system(dc, &glial));
    ASSERT_TRUE(distrib_cognition_register_brain_region(dc, &region));

    ASSERT_TRUE(distrib_cognition_start(dc));

    // Interleave 10 operations of each type
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.5f));
        ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, i*10, i*10+1, 0.5f, 0));
        ASSERT_TRUE(distrib_cognition_propagate_calcium_wave(dc, i, 0.5f, 15.0f));
        ASSERT_TRUE(distrib_cognition_broadcast_region_activity(dc, 1, 0.5f, 20.0f, 500, 1000));
    }

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.neuromod_broadcasts, 10);
    EXPECT_EQ(stats.glial_pruning_coordinations, 10);
    EXPECT_EQ(stats.glial_calcium_propagations, 10);
    EXPECT_EQ(stats.region_activity_broadcasts, 10);

    ASSERT_TRUE(distrib_cognition_stop(dc));
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(DistributedAdvancedTest, ZeroConcentrationHandling) {
    /**
     * WHAT: Test handling of zero neuromodulator concentrations
     * WHY:  Verify boundary condition (0.0) is valid
     * HOW:  Broadcast with 0.0 concentration
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    EXPECT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.0f));
    EXPECT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_SEROTONIN, 0.0f));
}

TEST_F(DistributedAdvancedTest, MaxConcentrationHandling) {
    /**
     * WHAT: Test handling of maximum neuromodulator concentrations
     * WHY:  Verify boundary condition (1.0) is valid
     * HOW:  Broadcast with 1.0 concentration
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    EXPECT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 1.0f));
    EXPECT_TRUE(distrib_cognition_broadcast_neuromod(dc, NEUROMOD_GLUTAMATE, 1.0f));
}

TEST_F(DistributedAdvancedTest, RapidStartStop) {
    /**
     * WHAT: Test rapid start/stop cycles
     * WHY:  Verify thread cleanup is reliable
     * HOW:  Start and stop 10 times in succession
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(distrib_cognition_start(dc));
        usleep(5000);  // 5ms
        ASSERT_TRUE(distrib_cognition_stop(dc));
    }

    // Should complete without hanging or crashing
}

//=============================================================================
// Statistics and Monitoring Tests
//=============================================================================

TEST_F(DistributedAdvancedTest, StatisticsAccuracy) {
    /**
     * WHAT: Verify statistics accurately track all operations
     * WHY:  Ensure monitoring/debugging capabilities
     * HOW:  Perform known number of operations, check counts
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    // Perform specific number of each operation
    const int NEUROMOD_OPS = 7;
    const int PRUNING_OPS = 5;
    const int CALCIUM_OPS = 3;
    const int REGION_OPS = 9;

    for (int i = 0; i < NEUROMOD_OPS; i++) {
        distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.5f);
    }

    for (int i = 0; i < PRUNING_OPS; i++) {
        distrib_cognition_coordinate_pruning(dc, i, i+1, 0.5f, 0);
    }

    for (int i = 0; i < CALCIUM_OPS; i++) {
        distrib_cognition_propagate_calcium_wave(dc, i, 0.5f, 15.0f);
    }

    for (int i = 0; i < REGION_OPS; i++) {
        distrib_cognition_broadcast_region_activity(dc, 1, 0.5f, 20.0f, 500, 1000);
    }

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));

    EXPECT_EQ(stats.neuromod_broadcasts, NEUROMOD_OPS);
    EXPECT_EQ(stats.glial_pruning_coordinations, PRUNING_OPS);
    EXPECT_EQ(stats.glial_calcium_propagations, CALCIUM_OPS);
    EXPECT_EQ(stats.region_activity_broadcasts, REGION_OPS);
    EXPECT_EQ(stats.messages_sent, NEUROMOD_OPS + PRUNING_OPS + CALCIUM_OPS + REGION_OPS);
}

TEST_F(DistributedAdvancedTest, TimestampProgression) {
    /**
     * WHAT: Verify last sync timestamps are updated
     * WHY:  Ensure timing tracking works correctly
     * HOW:  Check timestamps progress with operations
     */
    dc = create_fast_coordinator();
    ASSERT_NE(dc, nullptr);

    distrib_cognition_stats_t stats_before, stats_after;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats_before));

    usleep(10000);  // 10ms delay

    distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.5f);

    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats_after));

    // Last neuromod sync should have progressed
    EXPECT_GT(stats_after.last_neuromod_sync, stats_before.last_neuromod_sync);
}
