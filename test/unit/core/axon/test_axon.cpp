/**
 * @file test_axon.cpp
 * @brief Unit tests for NIMCP Axon Module
 *
 * WHAT: Comprehensive tests for axon creation, propagation, myelination
 * WHY:  Ensure correct action potential propagation and morphology
 * HOW:  GoogleTest framework with full code coverage
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/axon/nimcp_axon.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class AxonTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default test axon parameters
        test_id = 1;
        test_type = AXON_TYPE_MYELINATED;
        test_source = 100;
        test_target = 200;
        test_length = 1000.0f;  // 1mm
        test_diameter = 2.0f;   // 2um
    }

    void TearDown() override {
    }

    uint32_t test_id;
    axon_type_t test_type;
    uint32_t test_source;
    uint32_t test_target;
    float test_length;
    float test_diameter;
};

//=============================================================================
// CREATION AND DESTRUCTION TESTS
//=============================================================================

TEST_F(AxonTest, CreateDestroy_Basic) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    EXPECT_EQ(axon->id, test_id);
    EXPECT_EQ(axon->type, test_type);
    EXPECT_EQ(axon->source_neuron_id, test_source);
    EXPECT_EQ(axon->target_synapse_id, test_target);
    EXPECT_FLOAT_EQ(axon->length, test_length);
    EXPECT_FLOAT_EQ(axon->diameter, test_diameter);
    EXPECT_EQ(axon->state, AXON_STATE_RESTING);
    EXPECT_TRUE(axon->is_functional);

    axon_destroy(axon);
}

TEST_F(AxonTest, CreateDestroy_NullSafe) {
    // Should not crash on NULL
    axon_destroy(nullptr);
}

TEST_F(AxonTest, Create_InvalidLength) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                -1.0f, test_diameter);
    EXPECT_EQ(axon, nullptr);

    axon = axon_create(test_id, test_type, test_source, test_target,
                       0.0f, test_diameter);
    EXPECT_EQ(axon, nullptr);
}

TEST_F(AxonTest, Create_InvalidDiameter) {
    // Too small
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, 0.1f);
    EXPECT_EQ(axon, nullptr);

    // Too large
    axon = axon_create(test_id, test_type, test_source, test_target,
                       test_length, 25.0f);
    EXPECT_EQ(axon, nullptr);
}

TEST_F(AxonTest, CreateWithPositions) {
    float start[3] = {0.0f, 0.0f, 0.0f};
    float end[3] = {1000.0f, 0.0f, 0.0f};

    axon_t* axon = axon_create_with_positions(test_id, test_type, test_source,
                                               test_target, start, end, test_diameter);
    ASSERT_NE(axon, nullptr);

    EXPECT_FLOAT_EQ(axon->length, 1000.0f);
    EXPECT_FLOAT_EQ(axon->start_pos[0], 0.0f);
    EXPECT_FLOAT_EQ(axon->end_pos[0], 1000.0f);

    axon_destroy(axon);
}

TEST_F(AxonTest, CreateWithPositions_NullPositions) {
    float start[3] = {0.0f, 0.0f, 0.0f};

    axon_t* axon = axon_create_with_positions(test_id, test_type, test_source,
                                               test_target, nullptr, nullptr, test_diameter);
    EXPECT_EQ(axon, nullptr);

    axon = axon_create_with_positions(test_id, test_type, test_source,
                                       test_target, start, nullptr, test_diameter);
    EXPECT_EQ(axon, nullptr);
}

//=============================================================================
// AXON TYPE TESTS
//=============================================================================

TEST_F(AxonTest, TypeStrings) {
    EXPECT_STREQ(axon_type_to_string(AXON_TYPE_UNMYELINATED), "Unmyelinated");
    EXPECT_STREQ(axon_type_to_string(AXON_TYPE_MYELINATED), "Myelinated");
    EXPECT_STREQ(axon_type_to_string(AXON_TYPE_A_ALPHA), "A-alpha");
    EXPECT_STREQ(axon_type_to_string(AXON_TYPE_A_BETA), "A-beta");
    EXPECT_STREQ(axon_type_to_string(AXON_TYPE_A_DELTA), "A-delta");
    EXPECT_STREQ(axon_type_to_string(AXON_TYPE_C_FIBER), "C-fiber");
}

TEST_F(AxonTest, StateStrings) {
    EXPECT_STREQ(axon_state_to_string(AXON_STATE_RESTING), "Resting");
    EXPECT_STREQ(axon_state_to_string(AXON_STATE_ACTIVE), "Active");
    EXPECT_STREQ(axon_state_to_string(AXON_STATE_REFRACTORY), "Refractory");
    EXPECT_STREQ(axon_state_to_string(AXON_STATE_DEMYELINATING), "Demyelinating");
    EXPECT_STREQ(axon_state_to_string(AXON_STATE_DAMAGED), "Damaged");
}

TEST_F(AxonTest, AllAxonTypes) {
    for (int t = 0; t < AXON_TYPE_COUNT; t++) {
        axon_type_t type = static_cast<axon_type_t>(t);
        axon_t* axon = axon_create(test_id, type, test_source, test_target,
                                    test_length, test_diameter);
        ASSERT_NE(axon, nullptr) << "Failed for type " << t;
        EXPECT_EQ(axon->type, type);
        axon_destroy(axon);
    }
}

//=============================================================================
// SEGMENTATION TESTS
//=============================================================================

TEST_F(AxonTest, CreateSegments_Basic) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    bool result = axon_create_segments(axon, 10, 100.0f);
    EXPECT_TRUE(result);
    EXPECT_EQ(axon->num_segments, 10);
    EXPECT_NE(axon->segments, nullptr);

    // Check segment types
    EXPECT_EQ(axon->segments[0].type, SEGMENT_TYPE_AIS);
    EXPECT_EQ(axon->segments[9].type, SEGMENT_TYPE_TERMINAL);

    axon_destroy(axon);
}

TEST_F(AxonTest, CreateSegments_AlternatingPattern) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    axon_create_segments(axon, 8, 100.0f);

    // Check alternating node/internode pattern
    EXPECT_EQ(axon->segments[1].type, SEGMENT_TYPE_NODE);
    EXPECT_EQ(axon->segments[2].type, SEGMENT_TYPE_INTERNODE);
    EXPECT_EQ(axon->segments[3].type, SEGMENT_TYPE_NODE);
    EXPECT_EQ(axon->segments[4].type, SEGMENT_TYPE_INTERNODE);

    axon_destroy(axon);
}

TEST_F(AxonTest, CreateSegments_Invalid) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    // Zero segments
    EXPECT_FALSE(axon_create_segments(axon, 0, 100.0f));

    // Too many segments
    EXPECT_FALSE(axon_create_segments(axon, NIMCP_AXON_MAX_SEGMENTS + 1, 100.0f));

    // Invalid internode length
    EXPECT_FALSE(axon_create_segments(axon, 10, 0.0f));
    EXPECT_FALSE(axon_create_segments(axon, 10, -10.0f));

    // NULL axon
    EXPECT_FALSE(axon_create_segments(nullptr, 10, 100.0f));

    axon_destroy(axon);
}

TEST_F(AxonTest, SetSegmentMyelination) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    axon_create_segments(axon, 8, 100.0f);

    // Find an internode segment (index 2 should be internode)
    EXPECT_EQ(axon->segments[2].type, SEGMENT_TYPE_INTERNODE);

    float original_delay = axon->propagation_delay_ms;

    // Set high myelination
    bool result = axon_set_segment_myelination(axon, 2, 0.9f, 42);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(axon->segments[2].myelination, 0.9f);
    EXPECT_EQ(axon->segments[2].oligo_id, 42);

    // Delay should decrease with myelination
    EXPECT_LT(axon->propagation_delay_ms, original_delay);

    axon_destroy(axon);
}

TEST_F(AxonTest, SetSegmentMyelination_NodeFails) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    axon_create_segments(axon, 8, 100.0f);

    // Node at index 1 cannot be myelinated
    EXPECT_EQ(axon->segments[1].type, SEGMENT_TYPE_NODE);
    EXPECT_FALSE(axon_set_segment_myelination(axon, 1, 0.9f, 42));

    axon_destroy(axon);
}

//=============================================================================
// CONDUCTION VELOCITY TESTS
//=============================================================================

TEST_F(AxonTest, VelocityCalculation_Unmyelinated) {
    axon_t* axon = axon_create(test_id, AXON_TYPE_UNMYELINATED, test_source,
                                test_target, test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    float velocity = axon_calculate_velocity(axon);

    // Unmyelinated velocity ~ sqrt(diameter)
    float expected = 1.0f * sqrtf(test_diameter);
    EXPECT_NEAR(velocity, expected, 0.5f);

    axon_destroy(axon);
}

TEST_F(AxonTest, VelocityCalculation_Myelinated) {
    axon_t* axon = axon_create(test_id, AXON_TYPE_MYELINATED, test_source,
                                test_target, test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    // Set full myelination
    axon_set_myelination(axon, 1.0f);

    float velocity = axon_calculate_velocity(axon);

    // Myelinated should be much faster than unmyelinated
    axon_t* unmyelinated = axon_create(2, AXON_TYPE_UNMYELINATED, test_source,
                                        test_target, test_length, test_diameter);
    float unmyel_velocity = axon_calculate_velocity(unmyelinated);

    EXPECT_GT(velocity, unmyel_velocity * 5.0f);

    axon_destroy(axon);
    axon_destroy(unmyelinated);
}

TEST_F(AxonTest, VelocityCalculation_DiameterEffect) {
    // Larger diameter = faster conduction
    axon_t* small = axon_create(1, test_type, test_source, test_target,
                                 test_length, 1.0f);
    axon_t* large = axon_create(2, test_type, test_source, test_target,
                                 test_length, 5.0f);
    ASSERT_NE(small, nullptr);
    ASSERT_NE(large, nullptr);

    EXPECT_GT(axon_calculate_velocity(large), axon_calculate_velocity(small));

    axon_destroy(small);
    axon_destroy(large);
}

TEST_F(AxonTest, PropagationDelay) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    float delay = axon_get_propagation_delay(axon);

    // delay = length / (velocity * 1000)
    // For 1000um at ~6 m/s, delay ~ 0.17ms
    EXPECT_GT(delay, 0.0f);
    EXPECT_LT(delay, 10.0f);  // Should be < 10ms for 1mm axon

    axon_destroy(axon);
}

TEST_F(AxonTest, UpdateConduction) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    float initial_velocity = axon->effective_velocity;
    float initial_delay = axon->propagation_delay_ms;

    // Change myelination
    axon_set_myelination(axon, 0.9f);

    // Velocity should increase, delay should decrease
    EXPECT_NE(axon->effective_velocity, initial_velocity);
    EXPECT_NE(axon->propagation_delay_ms, initial_delay);

    axon_destroy(axon);
}

//=============================================================================
// SPIKE PROPAGATION TESTS
//=============================================================================

TEST_F(AxonTest, InitiateSpike_Basic) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    uint64_t time = 1000000;  // 1 second
    bool result = axon_initiate_spike(axon, time, 1.0f);

    EXPECT_TRUE(result);
    EXPECT_EQ(axon->state, AXON_STATE_ACTIVE);
    EXPECT_EQ(axon->activity.total_spikes, 1);

    axon_destroy(axon);
}

TEST_F(AxonTest, InitiateSpike_RefractoryBlock) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    uint64_t time = 1000000;

    // First spike succeeds
    EXPECT_TRUE(axon_initiate_spike(axon, time, 1.0f));

    // Immediate second spike fails (refractory)
    EXPECT_FALSE(axon_initiate_spike(axon, time + 100, 1.0f));

    // After refractory period, spike succeeds
    uint64_t post_refractory = time + (uint64_t)(NIMCP_AXON_REFRACTORY_PERIOD_MS * 1000.0f) + 1000;

    // Need to step to update state (ACTIVE -> REFRACTORY -> RESTING)
    axon_step(axon, post_refractory, 1.0f);
    axon_step(axon, post_refractory + 1000, 1.0f);

    // Spike should now succeed after state transitions
    uint64_t spike_time = post_refractory + 2000;
    EXPECT_TRUE(axon_initiate_spike(axon, spike_time, 1.0f));

    axon_destroy(axon);
}

TEST_F(AxonTest, InitiateSpike_DamagedAxon) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    axon->damage = 1.0f;
    axon->is_functional = false;

    EXPECT_FALSE(axon_initiate_spike(axon, 1000000, 1.0f));

    axon_destroy(axon);
}

TEST_F(AxonTest, InitiateSpike_LowATP) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    axon->atp_level = 0.05f;  // Below threshold

    EXPECT_FALSE(axon_initiate_spike(axon, 1000000, 1.0f));

    axon_destroy(axon);
}

TEST_F(AxonTest, SpikeArrived) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    uint64_t time = 1000000;
    axon_initiate_spike(axon, time, 1.0f);

    // Immediately after, spike hasn't arrived
    EXPECT_FALSE(axon_spike_arrived(axon, time + 1));

    // After delay, spike has arrived
    uint64_t delay_us = (uint64_t)(axon->propagation_delay_ms * 1000.0f);
    EXPECT_TRUE(axon_spike_arrived(axon, time + delay_us + 1));

    axon_destroy(axon);
}

TEST_F(AxonTest, IsRefractory) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    uint64_t time = 1000000;

    // Not refractory initially
    EXPECT_FALSE(axon_is_refractory(axon, time));

    // Refractory after spike
    axon_initiate_spike(axon, time, 1.0f);
    EXPECT_TRUE(axon_is_refractory(axon, time + 100));

    axon_destroy(axon);
}

//=============================================================================
// MYELINATION INTERFACE TESTS
//=============================================================================

TEST_F(AxonTest, SetMyelination) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    float original_velocity = axon->effective_velocity;

    axon_set_myelination(axon, 1.0f);
    EXPECT_FLOAT_EQ(axon->myelination_level, 1.0f);

    // Velocity should increase
    EXPECT_GT(axon->effective_velocity, original_velocity);

    // Test clamping
    axon_set_myelination(axon, 1.5f);
    EXPECT_FLOAT_EQ(axon->myelination_level, 1.0f);

    axon_set_myelination(axon, -0.5f);
    EXPECT_FLOAT_EQ(axon->myelination_level, 0.0f);

    axon_destroy(axon);
}

TEST_F(AxonTest, MyelinationSignal) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    // Initially low signal
    float initial_signal = axon_get_myelination_signal(axon);
    EXPECT_GE(initial_signal, 0.0f);
    EXPECT_LE(initial_signal, 1.0f);

    // After activity, signal increases
    for (int i = 0; i < 10; i++) {
        axon_initiate_spike(axon, 1000000 + i * 50000, 1.0f);
        axon_step(axon, 1000000 + i * 50000 + 2000, 1.0f);
    }
    axon_update_activity(axon, 1500000);

    float active_signal = axon_get_myelination_signal(axon);
    EXPECT_GE(active_signal, initial_signal);

    axon_destroy(axon);
}

TEST_F(AxonTest, ReceiveLactate) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    // Deplete ATP
    axon->atp_level = 0.2f;

    // Receive lactate
    axon_receive_lactate(axon, 0.5f);

    // ATP should increase
    EXPECT_GT(axon->atp_level, 0.2f);

    axon_destroy(axon);
}

//=============================================================================
// ACTIVITY TRACKING TESTS
//=============================================================================

TEST_F(AxonTest, ActivityStats) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    // Generate activity with proper state transitions
    uint64_t time = 1000000;
    uint64_t isi = 50000;  // 50ms between spikes

    for (int i = 0; i < 5; i++) {
        uint64_t spike_time = time + i * isi;

        // Step to ensure RESTING state before spike
        axon_step(axon, spike_time, 1.0f);

        // Initiate spike
        EXPECT_TRUE(axon_initiate_spike(axon, spike_time, 1.0f));

        // Step through ACTIVE -> REFRACTORY -> RESTING
        axon_step(axon, spike_time + 2000, 1.0f);   // ACTIVE -> REFRACTORY
        axon_step(axon, spike_time + 10000, 1.0f);  // REFRACTORY -> RESTING
    }

    axon_activity_stats_t stats;
    axon_get_activity_stats(axon, &stats);

    EXPECT_EQ(stats.total_spikes, 5);
    EXPECT_GT(stats.last_spike_time, 0);

    axon_destroy(axon);
}

TEST_F(AxonTest, FiringRate) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    // Initial rate is 0
    EXPECT_FLOAT_EQ(axon_get_firing_rate(axon), 0.0f);

    // Generate regular spikes (20ms ISI = 50 Hz)
    uint64_t time = 1000000;
    for (int i = 0; i < 10; i++) {
        axon_initiate_spike(axon, time + i * 20000, 1.0f);
        axon_step(axon, time + i * 20000 + 2000, 1.0f);
    }
    axon_update_activity(axon, time + 200000);

    float rate = axon_get_firing_rate(axon);
    EXPECT_GT(rate, 0.0f);

    axon_destroy(axon);
}

//=============================================================================
// STATE MANAGEMENT TESTS
//=============================================================================

TEST_F(AxonTest, Step) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    uint64_t time = 1000000;

    // Initiate spike
    axon_initiate_spike(axon, time, 1.0f);
    EXPECT_EQ(axon->state, AXON_STATE_ACTIVE);

    // Step past refractory period
    uint64_t post_refractory = time + 3000;  // 3ms later
    axon_step(axon, post_refractory, 1.0f);

    // State should transition
    EXPECT_NE(axon->state, AXON_STATE_ACTIVE);

    axon_destroy(axon);
}

TEST_F(AxonTest, DamageEffect) {
    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    float healthy_velocity = axon_calculate_velocity(axon);

    axon->damage = 0.5f;
    float damaged_velocity = axon_calculate_velocity(axon);

    // Damage reduces velocity
    EXPECT_LT(damaged_velocity, healthy_velocity);

    // Full damage makes axon non-functional
    axon->damage = 1.0f;
    axon_step(axon, 1000000, 1.0f);

    EXPECT_FALSE(axon->is_functional);
    EXPECT_EQ(axon->state, AXON_STATE_DAMAGED);

    axon_destroy(axon);
}

//=============================================================================
// SPIKE QUEUE TESTS
//=============================================================================

TEST_F(AxonTest, SpikeQueue_CreateDestroy) {
    axon_spike_queue_t* queue = axon_spike_queue_create(100);
    ASSERT_NE(queue, nullptr);

    EXPECT_EQ(axon_spike_queue_size(queue), 0);

    axon_spike_queue_destroy(queue);
}

TEST_F(AxonTest, SpikeQueue_PushPop) {
    axon_spike_queue_t* queue = axon_spike_queue_create(100);
    ASSERT_NE(queue, nullptr);

    axon_spike_event_t event = {
        .axon_id = 1,
        .initiation_time = 1000,
        .arrival_time = 2000,
        .amplitude = 1.0f,
        .source_neuron_id = 100,
        .target_synapse_id = 200
    };

    EXPECT_TRUE(axon_spike_queue_push(queue, &event));
    EXPECT_EQ(axon_spike_queue_size(queue), 1);

    axon_spike_event_t popped;
    EXPECT_TRUE(axon_spike_queue_pop(queue, 2000, &popped));
    EXPECT_EQ(popped.axon_id, 1);
    EXPECT_EQ(axon_spike_queue_size(queue), 0);

    axon_spike_queue_destroy(queue);
}

TEST_F(AxonTest, SpikeQueue_TimeFiltering) {
    axon_spike_queue_t* queue = axon_spike_queue_create(100);
    ASSERT_NE(queue, nullptr);

    // Push event arriving at time 5000
    axon_spike_event_t event = {
        .axon_id = 1,
        .initiation_time = 1000,
        .arrival_time = 5000,
        .amplitude = 1.0f,
        .source_neuron_id = 100,
        .target_synapse_id = 200
    };
    axon_spike_queue_push(queue, &event);

    // Pop at time 3000 - should fail (event hasn't arrived)
    axon_spike_event_t popped;
    EXPECT_FALSE(axon_spike_queue_pop(queue, 3000, &popped));

    // Pop at time 5000 - should succeed
    EXPECT_TRUE(axon_spike_queue_pop(queue, 5000, &popped));

    axon_spike_queue_destroy(queue);
}

//=============================================================================
// AXON NETWORK TESTS
//=============================================================================

TEST_F(AxonTest, Network_CreateDestroy) {
    axon_network_t* network = axon_network_create(100);
    ASSERT_NE(network, nullptr);

    axon_network_stats_t stats;
    axon_network_get_stats(network, &stats);
    EXPECT_EQ(stats.total_axons, 0);

    axon_network_destroy(network);
}

TEST_F(AxonTest, Network_AddRemove) {
    axon_network_t* network = axon_network_create(100);
    ASSERT_NE(network, nullptr);

    axon_t* axon = axon_create(test_id, test_type, test_source, test_target,
                                test_length, test_diameter);
    ASSERT_NE(axon, nullptr);

    EXPECT_TRUE(axon_network_add(network, axon));

    axon_network_stats_t stats;
    axon_network_get_stats(network, &stats);
    EXPECT_EQ(stats.total_axons, 1);

    axon_t* removed = axon_network_remove(network, test_id);
    EXPECT_EQ(removed, axon);

    axon_network_get_stats(network, &stats);
    EXPECT_EQ(stats.total_axons, 0);

    axon_destroy(removed);
    axon_network_destroy(network);
}

TEST_F(AxonTest, Network_Find) {
    axon_network_t* network = axon_network_create(100);
    ASSERT_NE(network, nullptr);

    for (uint32_t i = 0; i < 10; i++) {
        axon_t* axon = axon_create(i, test_type, i * 10, i * 10 + 1,
                                    test_length, test_diameter);
        axon_network_add(network, axon);
    }

    // Find by ID
    axon_t* found = axon_network_find(network, 5);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, 5);

    // Find by source neuron
    axon_t* results[10];
    uint32_t count = axon_network_find_by_source(network, 30, results, 10);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(results[0]->source_neuron_id, 30);

    // Not found
    EXPECT_EQ(axon_network_find(network, 999), nullptr);

    axon_network_destroy(network);
}

TEST_F(AxonTest, Network_Step) {
    axon_network_t* network = axon_network_create(100);
    ASSERT_NE(network, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        axon_t* axon = axon_create(i, test_type, i * 10, i * 10 + 1,
                                    test_length, test_diameter);
        axon_network_add(network, axon);
    }

    // Step the network
    axon_network_step(network, 1000000, 1.0f);

    // All axons should still be functional
    for (uint32_t i = 0; i < 5; i++) {
        axon_t* axon = axon_network_find(network, i);
        EXPECT_TRUE(axon->is_functional);
    }

    axon_network_destroy(network);
}

TEST_F(AxonTest, Network_Stats) {
    axon_network_t* network = axon_network_create(100);
    ASSERT_NE(network, nullptr);

    // Add mixed axons
    for (uint32_t i = 0; i < 10; i++) {
        axon_type_t type = (i < 5) ? AXON_TYPE_MYELINATED : AXON_TYPE_UNMYELINATED;
        axon_t* axon = axon_create(i, type, i * 10, i * 10 + 1,
                                    test_length, test_diameter);
        if (type == AXON_TYPE_MYELINATED) {
            axon_set_myelination(axon, 0.8f);
        }
        axon_network_add(network, axon);
    }

    axon_network_stats_t stats;
    axon_network_get_stats(network, &stats);

    EXPECT_EQ(stats.total_axons, 10);
    EXPECT_EQ(stats.myelinated_count, 5);
    EXPECT_EQ(stats.unmyelinated_count, 5);
    EXPECT_GT(stats.mean_velocity, 0.0f);
    EXPECT_GT(stats.mean_delay, 0.0f);

    axon_network_destroy(network);
}

//=============================================================================
// UTILITY FUNCTION TESTS
//=============================================================================

TEST_F(AxonTest, Distance3D) {
    float a[3] = {0.0f, 0.0f, 0.0f};
    float b[3] = {3.0f, 4.0f, 0.0f};

    float dist = axon_distance_3d(a, b);
    EXPECT_FLOAT_EQ(dist, 5.0f);

    // Same point
    EXPECT_FLOAT_EQ(axon_distance_3d(a, a), 0.0f);

    // 3D diagonal
    float c[3] = {1.0f, 1.0f, 1.0f};
    EXPECT_NEAR(axon_distance_3d(a, c), sqrtf(3.0f), 0.0001f);
}

TEST_F(AxonTest, ValidateParams) {
    EXPECT_TRUE(axon_validate_params(100.0f, 1.0f));
    EXPECT_TRUE(axon_validate_params(10000.0f, 10.0f));

    EXPECT_FALSE(axon_validate_params(-1.0f, 1.0f));
    EXPECT_FALSE(axon_validate_params(0.0f, 1.0f));
    EXPECT_FALSE(axon_validate_params(100.0f, 0.1f));  // Too small
    EXPECT_FALSE(axon_validate_params(100.0f, 25.0f));  // Too large
}

//=============================================================================
// NULL SAFETY TESTS
//=============================================================================

TEST_F(AxonTest, NullSafety) {
    // All functions should handle NULL gracefully
    axon_destroy(nullptr);
    EXPECT_FALSE(axon_create_segments(nullptr, 10, 100.0f));
    EXPECT_FALSE(axon_set_segment_myelination(nullptr, 0, 0.5f, 1));
    EXPECT_FALSE(axon_initiate_spike(nullptr, 1000, 1.0f));
    EXPECT_FALSE(axon_spike_arrived(nullptr, 1000));
    EXPECT_FLOAT_EQ(axon_get_propagation_delay(nullptr), 0.0f);
    axon_set_myelination(nullptr, 0.5f);
    EXPECT_FLOAT_EQ(axon_get_myelination_signal(nullptr), 0.0f);
    axon_receive_lactate(nullptr, 0.5f);
    axon_update_activity(nullptr, 1000);
    EXPECT_FLOAT_EQ(axon_get_firing_rate(nullptr), 0.0f);

    axon_activity_stats_t stats;
    axon_get_activity_stats(nullptr, &stats);

    axon_step(nullptr, 1000, 1.0f);
    EXPECT_FALSE(axon_is_refractory(nullptr, 1000));
    EXPECT_FLOAT_EQ(axon_calculate_velocity(nullptr), 0.1f);  // MIN_VELOCITY
    axon_update_conduction(nullptr);

    // Network functions
    axon_network_destroy(nullptr);
    EXPECT_FALSE(axon_network_add(nullptr, nullptr));
    EXPECT_EQ(axon_network_remove(nullptr, 0), nullptr);
    EXPECT_EQ(axon_network_find(nullptr, 0), nullptr);
    EXPECT_EQ(axon_network_find_by_source(nullptr, 0, nullptr, 0), 0);
    axon_network_step(nullptr, 1000, 1.0f);

    axon_network_stats_t net_stats;
    axon_network_get_stats(nullptr, &net_stats);

    // Queue functions
    axon_spike_queue_destroy(nullptr);
    EXPECT_FALSE(axon_spike_queue_push(nullptr, nullptr));

    axon_spike_event_t event;
    EXPECT_FALSE(axon_spike_queue_pop(nullptr, 1000, &event));
    EXPECT_EQ(axon_spike_queue_size(nullptr), 0);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
