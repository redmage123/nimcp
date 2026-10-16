/**
 * @file test_axon_integration.cpp
 * @brief Integration tests for NIMCP Axon Module
 *
 * WHAT: Test axon integration with oligodendrocytes and brain
 * WHY:  Ensure proper interaction between modules
 * HOW:  GoogleTest with cross-module scenarios
 */

#include <gtest/gtest.h>
#include <vector>

// Headers have their own extern "C" guards
#include "core/axon/nimcp_axon.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"

//=============================================================================
// AXON-OLIGODENDROCYTE INTEGRATION
//=============================================================================

class AxonOligoIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create axon network
        axon_net = axon_network_create(100);
        ASSERT_NE(axon_net, nullptr);

        // Create oligodendrocyte network
        oligo_net = oligodendrocyte_network_create(10);
        ASSERT_NE(oligo_net, nullptr);
    }

    void TearDown() override {
        if (axon_net) axon_network_destroy(axon_net);
        if (oligo_net) oligodendrocyte_network_destroy(oligo_net);
    }

    axon_network_t* axon_net = nullptr;
    oligodendrocyte_network_t* oligo_net = nullptr;
};

TEST_F(AxonOligoIntegrationTest, MyelinationAffectsVelocity) {
    // Create unmyelinated axon first to get baseline
    axon_t* unmyelinated = axon_create(1, AXON_TYPE_UNMYELINATED, 100, 200, 1000.0f, 2.0f);
    ASSERT_NE(unmyelinated, nullptr);
    float unmyelinated_velocity = unmyelinated->effective_velocity;

    // Create myelinated axon
    axon_t* axon = axon_create(2, AXON_TYPE_MYELINATED, 100, 200, 1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);
    axon_network_add(axon_net, axon);

    // Create oligodendrocyte and assign axon
    oligodendrocyte_t* oligo = oligodendrocyte_create(1, 0.0f, 0.0f, 0.0f, 10);
    ASSERT_NE(oligo, nullptr);
    oligodendrocyte_network_add(oligo_net, oligo);

    // Set high myelination level
    axon_set_myelination(axon, 1.0f);

    float myelinated_velocity = axon->effective_velocity;

    // Myelinated velocity should be significantly faster than unmyelinated
    // v_unmyelinated = 1.0 * sqrt(2.0) = 1.41 m/s
    // v_myelinated = 6.0 * 2.0 * 1.0 = 12.0 m/s
    // Ratio should be > 5x
    EXPECT_GT(myelinated_velocity, unmyelinated_velocity * 5.0f);

    axon_destroy(unmyelinated);
}

TEST_F(AxonOligoIntegrationTest, ActivityDrivenMyelination) {
    // Create axon
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200, 1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);
    axon_network_add(axon_net, axon);

    // Generate activity
    uint64_t time = 1000000;
    for (int i = 0; i < 20; i++) {
        axon_initiate_spike(axon, time + i * 20000, 1.0f);
        axon_step(axon, time + i * 20000 + 1500, 1.0f);
    }
    axon_update_activity(axon, time + 500000);

    // Get myelination signal for oligodendrocyte
    float signal = axon_get_myelination_signal(axon);

    // Active axon should generate myelination signal
    EXPECT_GT(signal, 0.0f);
}

TEST_F(AxonOligoIntegrationTest, LactateShuttle) {
    // Create axon with depleted ATP
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200, 1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);
    axon->atp_level = 0.3f;  // Low ATP

    // Simulate lactate shuttle from oligodendrocyte
    axon_receive_lactate(axon, 0.5f);

    // ATP should recover
    EXPECT_GT(axon->atp_level, 0.4f);

    // Axon should be able to spike
    EXPECT_TRUE(axon_initiate_spike(axon, 1000000, 1.0f));

    axon_destroy(axon);
}

TEST_F(AxonOligoIntegrationTest, SegmentedMyelination) {
    // Create segmented axon
    axon_t* axon = axon_create(1, AXON_TYPE_MYELINATED, 100, 200, 1000.0f, 2.0f);
    ASSERT_NE(axon, nullptr);
    axon_create_segments(axon, 10, 100.0f);

    // Different oligodendrocytes myelinate different segments
    for (uint32_t i = 0; i < axon->num_segments; i++) {
        if (axon->segments[i].type == SEGMENT_TYPE_INTERNODE) {
            uint32_t oligo_id = (i / 2) + 1;  // Different oligo per pair
            float myelin_level = 0.7f + (i % 3) * 0.1f;  // Varying levels
            axon_set_segment_myelination(axon, i, myelin_level, oligo_id);
        }
    }

    // Axon should have variable myelination
    float min_myelin = 1.0f, max_myelin = 0.0f;
    for (uint32_t i = 0; i < axon->num_segments; i++) {
        if (axon->segments[i].type == SEGMENT_TYPE_INTERNODE) {
            if (axon->segments[i].myelination < min_myelin) {
                min_myelin = axon->segments[i].myelination;
            }
            if (axon->segments[i].myelination > max_myelin) {
                max_myelin = axon->segments[i].myelination;
            }
        }
    }

    EXPECT_GT(max_myelin, min_myelin);  // Variable myelination

    axon_destroy(axon);
}

//=============================================================================
// NETWORK PROPAGATION TESTS
//=============================================================================

class AxonNetworkPropagationTest : public ::testing::Test {
protected:
    void SetUp() override {
        network = axon_network_create(100);
        ASSERT_NE(network, nullptr);
    }

    void TearDown() override {
        if (network) axon_network_destroy(network);
    }

    axon_network_t* network = nullptr;
};

TEST_F(AxonNetworkPropagationTest, SpikeDelivery) {
    // Create chain of axons
    for (uint32_t i = 0; i < 10; i++) {
        axon_t* axon = axon_create(i, AXON_TYPE_MYELINATED, i, i + 1,
                                    500.0f, 2.0f);
        ASSERT_NE(axon, nullptr);
        axon_set_myelination(axon, 0.8f);
        axon_network_add(network, axon);
    }

    // Initiate spike on first axon
    axon_t* first = axon_network_find(network, 0);
    ASSERT_NE(first, nullptr);

    uint64_t time = 1000000;
    axon_initiate_spike(first, time, 1.0f);

    // Step network
    for (int step = 0; step < 100; step++) {
        time += 100;  // 0.1ms steps
        axon_network_step(network, time, 0.1f);
    }

    // First axon should have transmitted spike
    EXPECT_EQ(first->activity.total_spikes, 1);
}

TEST_F(AxonNetworkPropagationTest, DelayDistribution) {
    // Create axons with different lengths
    std::vector<float> delays;

    for (uint32_t i = 0; i < 20; i++) {
        float length = 100.0f + i * 100.0f;  // 100um to 2mm
        axon_t* axon = axon_create(i, AXON_TYPE_MYELINATED, i, i + 100,
                                    length, 2.0f);
        ASSERT_NE(axon, nullptr);
        axon_set_myelination(axon, 0.8f);
        axon_network_add(network, axon);

        delays.push_back(axon_get_propagation_delay(axon));
    }

    // Delays should increase with length
    for (size_t i = 1; i < delays.size(); i++) {
        EXPECT_GT(delays[i], delays[i - 1]);
    }
}

TEST_F(AxonNetworkPropagationTest, MixedTypeNetwork) {
    // Create mixed network with different axon types
    for (uint32_t i = 0; i < 30; i++) {
        axon_type_t type;
        if (i < 10) type = AXON_TYPE_A_ALPHA;
        else if (i < 20) type = AXON_TYPE_MYELINATED;
        else type = AXON_TYPE_C_FIBER;

        float diameter = (type == AXON_TYPE_A_ALPHA) ? 15.0f :
                        (type == AXON_TYPE_MYELINATED) ? 2.0f : 0.5f;

        axon_t* axon = axon_create(i, type, i, i + 100, 1000.0f, diameter);
        ASSERT_NE(axon, nullptr);

        if (type != AXON_TYPE_C_FIBER) {
            axon_set_myelination(axon, 0.9f);
        }

        axon_network_add(network, axon);
    }

    axon_network_stats_t stats;
    axon_network_get_stats(network, &stats);

    EXPECT_EQ(stats.total_axons, 30);
    EXPECT_GT(stats.myelinated_count, 0);
    EXPECT_GT(stats.unmyelinated_count, 0);

    // A-alpha should have highest mean velocity
    float a_alpha_velocity = 0.0f;
    float c_fiber_velocity = 0.0f;

    for (uint32_t i = 0; i < 10; i++) {
        axon_t* axon = axon_network_find(network, i);
        a_alpha_velocity += axon->effective_velocity;
    }
    a_alpha_velocity /= 10;

    for (uint32_t i = 20; i < 30; i++) {
        axon_t* axon = axon_network_find(network, i);
        c_fiber_velocity += axon->effective_velocity;
    }
    c_fiber_velocity /= 10;

    EXPECT_GT(a_alpha_velocity, c_fiber_velocity * 10.0f);
}

//=============================================================================
// SPIKE QUEUE INTEGRATION
//=============================================================================

TEST_F(AxonNetworkPropagationTest, SpikeQueueProcessing) {
    // Create axons
    for (uint32_t i = 0; i < 5; i++) {
        axon_t* axon = axon_create(i, AXON_TYPE_MYELINATED, i, i + 100,
                                    500.0f, 2.0f);
        axon_set_myelination(axon, 0.8f);
        axon_network_add(network, axon);
    }

    // Queue spikes with different arrival times
    axon_spike_event_t events[5];
    for (int i = 0; i < 5; i++) {
        events[i].axon_id = i;
        events[i].initiation_time = 1000000;
        events[i].arrival_time = 1000000 + (i + 1) * 1000;  // Staggered arrivals
        events[i].amplitude = 1.0f;
        events[i].source_neuron_id = i;
        events[i].target_synapse_id = i + 100;

        axon_spike_queue_push(network->spike_queue, &events[i]);
    }

    EXPECT_EQ(axon_spike_queue_size(network->spike_queue), 5);

    // Process arrivals at different times
    int delivered_count = 0;
    auto callback = [](axon_t* axon, const axon_spike_event_t* spike, void* data) {
        int* count = (int*)data;
        (*count)++;
    };

    // At time 1002000, first 2 should arrive
    uint32_t delivered = axon_network_process_arrivals(network, 1002000,
                                                        callback, &delivered_count);
    EXPECT_EQ(delivered, 2);
    EXPECT_EQ(delivered_count, 2);

    // At time 1010000, all remaining should arrive
    delivered = axon_network_process_arrivals(network, 1010000,
                                               callback, &delivered_count);
    EXPECT_EQ(delivered, 3);
    EXPECT_EQ(delivered_count, 5);
}

//=============================================================================
// LONG-RUNNING SIMULATION
//=============================================================================

TEST_F(AxonNetworkPropagationTest, LongTermSimulation) {
    // Create realistic network
    for (uint32_t i = 0; i < 50; i++) {
        float length = 200.0f + (i % 10) * 100.0f;
        float diameter = 1.0f + (i % 5) * 0.5f;

        axon_t* axon = axon_create(i, AXON_TYPE_MYELINATED, i, i + 100,
                                    length, diameter);
        axon_set_myelination(axon, 0.5f + (i % 5) * 0.1f);
        axon_network_add(network, axon);
    }

    // Simulate 1 second (1000 ms)
    uint64_t time = 0;
    uint32_t total_spikes = 0;

    for (int step = 0; step < 1000; step++) {
        time += 1000;  // 1ms steps

        // Random axon fires every 20ms on average
        if (step % 20 == 0) {
            uint32_t axon_id = step % 50;
            axon_t* axon = axon_network_find(network, axon_id);
            if (axon && axon_initiate_spike(axon, time, 1.0f)) {
                total_spikes++;
            }
        }

        axon_network_step(network, time, 1.0f);
    }

    // Network should have processed spikes
    EXPECT_GT(total_spikes, 0);

    // All axons should still be functional
    axon_network_stats_t stats;
    axon_network_get_stats(network, &stats);
    EXPECT_EQ(stats.damaged_count, 0);
    EXPECT_EQ(stats.total_axons, 50);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
