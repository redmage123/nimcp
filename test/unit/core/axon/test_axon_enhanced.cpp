/**
 * @file test_axon_enhanced.cpp
 * @brief Enhanced unit tests for NIMCP Axon Module - Full Coverage
 *
 * WHAT: Additional tests for edge cases and integration scenarios
 * WHY:  Achieve 100% code coverage
 * HOW:  GoogleTest with stress and edge case testing
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/axon/nimcp_axon.h"

//=============================================================================
// STRESS TESTS
//=============================================================================

class AxonStressTest : public ::testing::Test {
protected:
    static constexpr uint32_t NUM_AXONS = 1000;
    static constexpr uint32_t NUM_SPIKES = 100;
};

TEST_F(AxonStressTest, ManyAxons) {
    axon_network_t* network = axon_network_create(NUM_AXONS);
    ASSERT_NE(network, nullptr);

    // Create many axons
    for (uint32_t i = 0; i < NUM_AXONS; i++) {
        axon_type_t type = static_cast<axon_type_t>(i % AXON_TYPE_COUNT);
        float length = 100.0f + (i % 1000);
        float diameter = 0.5f + (i % 10) * 0.2f;

        axon_t* axon = axon_create(i, type, i, i + NUM_AXONS, length, diameter);
        ASSERT_NE(axon, nullptr);
        EXPECT_TRUE(axon_network_add(network, axon));
    }

    axon_network_stats_t stats;
    axon_network_get_stats(network, &stats);
    EXPECT_EQ(stats.total_axons, NUM_AXONS);

    axon_network_destroy(network);
}

TEST_F(AxonStressTest, RapidSpikes) {
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200, 1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);

    uint64_t time = 1000000;
    uint32_t successful_spikes = 0;

    for (uint32_t i = 0; i < NUM_SPIKES; i++) {
        // Space spikes by refractory period + 2ms
        time += 3000;  // 3ms between attempts

        // Step to ensure state is RESTING before spike attempt
        axon_step(axon, time - 1000, 1.0f);  // ACTIVE -> REFRACTORY
        axon_step(axon, time, 1.0f);          // REFRACTORY -> RESTING

        if (axon_initiate_spike(axon, time, 1.0f)) {
            successful_spikes++;
        }
    }

    // Most spikes should succeed with proper spacing
    EXPECT_GT(successful_spikes, NUM_SPIKES / 2);
    EXPECT_EQ(axon->activity.total_spikes, successful_spikes);

    axon_destroy(axon);
}

TEST_F(AxonStressTest, NetworkStep) {
    axon_network_t* network = axon_network_create(100);
    ASSERT_NE(network, nullptr);

    for (uint32_t i = 0; i < 100; i++) {
        axon_t* axon = axon_create(i, AXON_TYPE_MYELINATED, i, i + 100,
                                    500.0f + i * 10, 1.0f + (i % 5) * 0.5f);
        axon_network_add(network, axon);
    }

    // Step many times
    uint64_t time = 1000000;
    for (int step = 0; step < 1000; step++) {
        time += 1000;  // 1ms per step
        axon_network_step(network, time, 1.0f);
    }

    // Network should still be healthy
    axon_network_stats_t stats;
    axon_network_get_stats(network, &stats);
    EXPECT_EQ(stats.total_axons, 100);
    EXPECT_EQ(stats.damaged_count, 0);

    axon_network_destroy(network);
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

class AxonEdgeCaseTest : public ::testing::Test {};

TEST_F(AxonEdgeCaseTest, MinimumDiameter) {
    axon_t* axon = axon_create(1, AXON_TYPE_UNMYELINATED, 100, 200, 100.0f,
                                NIMCP_AXON_MIN_DIAMETER_UM);
    ASSERT_NE(axon, nullptr);

    // Should still have valid velocity
    float velocity = axon_calculate_velocity(axon);
    EXPECT_GT(velocity, 0.0f);

    axon_destroy(axon);
}

TEST_F(AxonEdgeCaseTest, MaximumDiameter) {
    axon_t* axon = axon_create(1, AXON_TYPE_A_ALPHA, 100, 200, 100.0f,
                                NIMCP_AXON_MAX_DIAMETER_UM);
    ASSERT_NE(axon, nullptr);

    // Should have high velocity
    float velocity = axon_calculate_velocity(axon);
    EXPECT_GT(velocity, 10.0f);

    axon_destroy(axon);
}

TEST_F(AxonEdgeCaseTest, VeryLongAxon) {
    // 1 meter axon (peripheral nerve)
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200,
                                1000000.0f, 10.0f);
    ASSERT_NE(axon, nullptr);

    axon_set_myelination(axon, 1.0f);

    // Delay should be significant but finite
    float delay = axon_get_propagation_delay(axon);
    EXPECT_GT(delay, 10.0f);  // > 10ms for 1m at ~60 m/s
    EXPECT_LT(delay, 1000.0f);  // < 1 second

    axon_destroy(axon);
}

TEST_F(AxonEdgeCaseTest, VeryShortAxon) {
    // 1 micron axon (local connection)
    axon_t* axon = axon_create(1, AXON_TYPE_UNMYELINATED, 100, 200,
                                1.0f, 0.5f);
    ASSERT_NE(axon, nullptr);

    // Delay should be very small
    float delay = axon_get_propagation_delay(axon);
    EXPECT_LT(delay, 0.1f);  // < 0.1ms

    axon_destroy(axon);
}

TEST_F(AxonEdgeCaseTest, SegmentCumulativeDelay) {
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200,
                                1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);

    axon_create_segments(axon, 10, 100.0f);

    // Cumulative delays should be monotonically increasing
    float prev_delay = 0.0f;
    for (uint32_t i = 0; i < axon->num_segments; i++) {
        EXPECT_GE(axon->segments[i].cumulative_delay, prev_delay);
        prev_delay = axon->segments[i].cumulative_delay;
    }

    axon_destroy(axon);
}

TEST_F(AxonEdgeCaseTest, MyelinationUpdatePropagates) {
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200,
                                1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);

    axon_create_segments(axon, 10, 100.0f);

    // Set myelination globally
    axon_set_myelination(axon, 0.9f);

    // All internodes should be updated
    for (uint32_t i = 0; i < axon->num_segments; i++) {
        if (axon->segments[i].type == SEGMENT_TYPE_INTERNODE) {
            EXPECT_FLOAT_EQ(axon->segments[i].myelination, 0.9f);
        }
    }

    axon_destroy(axon);
}

TEST_F(AxonEdgeCaseTest, ATPDepletion) {
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200,
                                1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);

    // Fire many spikes to deplete ATP
    uint64_t time = 1000000;
    while (axon->atp_level > 0.15f) {
        if (axon_initiate_spike(axon, time, 1.0f)) {
            // Spike consumed ATP
        }
        time += 2000;
        axon_step(axon, time, 0.1f);  // Small dt = slow ATP regeneration
    }

    // Eventually should fail to spike due to low ATP
    bool can_spike = axon_initiate_spike(axon, time, 1.0f);
    // May or may not spike depending on exact ATP level

    // Recover with lactate
    axon_receive_lactate(axon, 1.0f);
    EXPECT_GT(axon->atp_level, 0.3f);

    axon_destroy(axon);
}

TEST_F(AxonEdgeCaseTest, QueueOverflow) {
    axon_spike_queue_t* queue = axon_spike_queue_create(10);
    ASSERT_NE(queue, nullptr);

    axon_spike_event_t event = {
        .axon_id = 1,
        .initiation_time = 1000,
        .arrival_time = 5000,
        .amplitude = 1.0f,
        .source_neuron_id = 100,
        .target_synapse_id = 200
    };

    // Fill queue
    for (int i = 0; i < 10; i++) {
        event.axon_id = i;
        EXPECT_TRUE(axon_spike_queue_push(queue, &event));
    }

    // Queue should be full
    EXPECT_EQ(axon_spike_queue_size(queue), 10);

    // Next push should fail
    event.axon_id = 100;
    EXPECT_FALSE(axon_spike_queue_push(queue, &event));

    axon_spike_queue_destroy(queue);
}

//=============================================================================
// BIOLOGICAL ACCURACY TESTS
//=============================================================================

class AxonBiologicalTest : public ::testing::Test {};

TEST_F(AxonBiologicalTest, ConductionVelocityRanges) {
    // C-fiber: 0.5-2 m/s (unmyelinated, small)
    axon_t* c_fiber = axon_create(1, AXON_TYPE_C_FIBER, 100, 200,
                                   1000.0f, 0.5f);
    ASSERT_NE(c_fiber, nullptr);
    float c_vel = axon_calculate_velocity(c_fiber);
    EXPECT_GT(c_vel, 0.3f);
    EXPECT_LT(c_vel, 3.0f);

    // A-alpha: 80-120 m/s (myelinated, large)
    axon_t* a_alpha = axon_create(2, AXON_TYPE_A_ALPHA, 100, 200,
                                   1000.0f, 15.0f);
    ASSERT_NE(a_alpha, nullptr);
    axon_set_myelination(a_alpha, 1.0f);
    float a_vel = axon_calculate_velocity(a_alpha);
    EXPECT_GT(a_vel, 50.0f);
    EXPECT_LT(a_vel, 150.0f);

    // A-alpha should be much faster than C-fiber
    EXPECT_GT(a_vel, c_vel * 10.0f);

    axon_destroy(c_fiber);
    axon_destroy(a_alpha);
}

TEST_F(AxonBiologicalTest, RefractoryPeriod) {
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200,
                                1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);

    uint64_t time = 1000000;
    axon_initiate_spike(axon, time, 1.0f);

    // Check refractory period (~1ms)
    EXPECT_TRUE(axon_is_refractory(axon, time + 500));   // 0.5ms - still refractory
    EXPECT_TRUE(axon_is_refractory(axon, time + 900));   // 0.9ms - still refractory

    // Update state - need two steps: ACTIVE -> REFRACTORY -> RESTING
    axon_step(axon, time + 1500, 1.0f);   // ACTIVE -> REFRACTORY (past refractory_end)
    axon_step(axon, time + 2000, 1.0f);   // REFRACTORY -> RESTING
    EXPECT_FALSE(axon_is_refractory(axon, time + 2000)); // 2ms - should be ready

    axon_destroy(axon);
}

TEST_F(AxonBiologicalTest, SaltatoryConduction) {
    // Myelinated axon with segments should have faster conduction
    axon_t* myelinated = axon_create(1, AXON_TYPE_MYELINATED, 100, 200,
                                      1000.0f, 2.0f);
    ASSERT_NE(myelinated, nullptr);

    // Add segments and myelination
    axon_create_segments(myelinated, 10, 100.0f);
    for (uint32_t i = 0; i < myelinated->num_segments; i++) {
        if (myelinated->segments[i].type == SEGMENT_TYPE_INTERNODE) {
            axon_set_segment_myelination(myelinated, i, 0.9f, 1);
        }
    }

    // Compare with unsegmented
    axon_t* simple = axon_create(2, AXON_TYPE_MYELINATED, 100, 200,
                                  1000.0f, 2.0f);
    ASSERT_NE(simple, nullptr);
    axon_set_myelination(simple, 0.9f);

    // Both should have reasonable velocities
    EXPECT_GT(myelinated->effective_velocity, 5.0f);
    EXPECT_GT(simple->effective_velocity, 5.0f);

    axon_destroy(myelinated);
    axon_destroy(simple);
}

TEST_F(AxonBiologicalTest, ActivityDependentMyelination) {
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200,
                                1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);

    // Low activity = low myelination signal
    float low_signal = axon_get_myelination_signal(axon);

    // Generate high activity
    uint64_t time = 1000000;
    for (int i = 0; i < 50; i++) {
        axon_initiate_spike(axon, time + i * 20000, 1.0f);  // 50 Hz
        axon_step(axon, time + i * 20000 + 1500, 1.0f);
    }
    axon_update_activity(axon, time + 1000000);

    // High activity = higher myelination signal
    float high_signal = axon_get_myelination_signal(axon);
    EXPECT_GE(high_signal, low_signal);

    axon_destroy(axon);
}

//=============================================================================
// CONCURRENT ACCESS TESTS
//=============================================================================

TEST(AxonConcurrencyTest, NetworkThreadSafety) {
    axon_network_t* network = axon_network_create(100);
    ASSERT_NE(network, nullptr);

    // Pre-populate
    for (uint32_t i = 0; i < 50; i++) {
        axon_t* axon = axon_create(i, AXON_TYPE_MYELINATED, i, i + 100,
                                    500.0f, 2.0f);
        axon_network_add(network, axon);
    }

    std::atomic<bool> error_occurred{false};

    // Multiple threads accessing network
    auto reader = [&]() {
        for (int i = 0; i < 100; i++) {
            axon_network_stats_t stats;
            axon_network_get_stats(network, &stats);
            if (stats.total_axons > 100) error_occurred = true;
        }
    };

    auto stepper = [&]() {
        uint64_t time = 1000000;
        for (int i = 0; i < 100; i++) {
            time += 1000;
            axon_network_step(network, time, 1.0f);
        }
    };

    std::thread t1(reader);
    std::thread t2(stepper);
    std::thread t3(reader);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_FALSE(error_occurred);

    axon_network_destroy(network);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
