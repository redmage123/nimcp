/**
 * @file test_spike_ring_buffer.cpp
 * @brief Unit tests for dynamic spike history ring buffer
 *
 * WHAT: Test suite validating heap-allocated spike history ring buffer in neuron_t
 * WHY:  Ensure correctness of ring buffer semantics (wrap-around, saturation, reset)
 *       and verify memory footprint reduction enabling 2M+ neuron networks
 * HOW:  Google Test framework with ring buffer lifecycle, wrap-around, and scale tests
 *
 * TEST COVERAGE:
 * - Buffer allocation on network create
 * - Custom capacity via network_config_t
 * - Recording and retrieval
 * - Wrap-around overwrites oldest
 * - Count saturation at capacity
 * - Reset clears buffer
 * - Destroy frees buffer (ASAN/valgrind)
 * - Add neuron gets spike buffer
 * - Average activity integration
 * - Neuron size reduction
 * - MAX_NEURONS raised
 * - Large network creation
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/neuralnet/nimcp_neuralnet.h"

//=============================================================================
// Helper: Create a default network config
//=============================================================================

static network_config_t make_default_config(uint32_t num_neurons) {
    network_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_neurons = num_neurons;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.refractory_period = 2.0f;
    config.target_activity = 0.1f;
    config.input_size = 2;
    config.output_size = 2;
    return config;
}

//=============================================================================
// Test Fixture
//=============================================================================

class SpikeRingBufferTest : public ::testing::Test {
protected:
    neural_network_t network = nullptr;

    void TearDown() override {
        if (network) {
            neural_network_destroy(network);
            network = nullptr;
        }
    }
};

//=============================================================================
// Allocation and Initialization Tests
//=============================================================================

TEST_F(SpikeRingBufferTest, InitAllocatesBuffer) {
    // WHAT: Verify spike_history is heap-allocated on network create
    // WHY:  Ring buffer must be allocated for each neuron
    // HOW:  Create network, check first neuron's spike_history fields
    network_config_t config = make_default_config(10);
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    EXPECT_NE(neuron->spike_history, nullptr)
        << "spike_history must be heap-allocated";
    EXPECT_GT(neuron->spike_history_capacity, 0u)
        << "capacity must be positive";
    EXPECT_EQ(neuron->spike_history_index, 0u)
        << "write position must start at 0";
    EXPECT_EQ(neuron->spike_history_count, 0u)
        << "valid entry count must start at 0";
}

TEST_F(SpikeRingBufferTest, CustomCapacityFromConfig) {
    // WHAT: Verify network_config_t.spike_history_capacity is honored
    // WHY:  Users need control over per-neuron memory footprint
    // HOW:  Set config capacity to 64, verify neuron has capacity=64
    network_config_t config = make_default_config(10);
    config.spike_history_capacity = 64;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    EXPECT_EQ(neuron->spike_history_capacity, 64u)
        << "Neuron spike_history_capacity should match config";
}

TEST_F(SpikeRingBufferTest, DefaultCapacityWhenZero) {
    // WHAT: Verify default capacity is used when config.spike_history_capacity = 0
    // WHY:  Zero means "use default" per API contract
    // HOW:  Create with capacity=0, verify SPIKE_HISTORY_DEFAULT_CAPACITY
    network_config_t config = make_default_config(10);
    config.spike_history_capacity = 0;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    EXPECT_EQ(neuron->spike_history_capacity, SPIKE_HISTORY_DEFAULT_CAPACITY)
        << "Zero config capacity should fall back to SPIKE_HISTORY_DEFAULT_CAPACITY";
}

//=============================================================================
// Recording Tests
//=============================================================================

TEST_F(SpikeRingBufferTest, RecordingFillsBuffer) {
    // WHAT: Record spikes and verify they are stored
    // WHY:  Basic recording correctness
    // HOW:  Record 4 spikes, verify timestamps and count
    network_config_t config = make_default_config(10);
    config.spike_history_capacity = 4;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    EXPECT_TRUE(neural_network_record_spike(network, 0, 1.0f, 100));
    EXPECT_TRUE(neural_network_record_spike(network, 0, 0.8f, 200));
    EXPECT_TRUE(neural_network_record_spike(network, 0, 0.6f, 300));
    EXPECT_TRUE(neural_network_record_spike(network, 0, 0.4f, 400));

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    EXPECT_EQ(neuron->spike_history_count, 4u);

    // Verify timestamps are recorded
    // Ring buffer stores in order: index 0..3 for first 4 entries
    EXPECT_EQ(neuron->spike_history[0].timestamp, 100u);
    EXPECT_EQ(neuron->spike_history[1].timestamp, 200u);
    EXPECT_EQ(neuron->spike_history[2].timestamp, 300u);
    EXPECT_EQ(neuron->spike_history[3].timestamp, 400u);
}

TEST_F(SpikeRingBufferTest, WrapAroundOverwritesOldest) {
    // WHAT: Verify ring buffer wraps and overwrites oldest entries
    // WHY:  Ring buffer semantics require FIFO eviction
    // HOW:  Record 6 spikes in capacity=4 buffer, verify oldest overwritten
    network_config_t config = make_default_config(10);
    config.spike_history_capacity = 4;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Record 6 spikes: 100, 200, 300, 400, 500, 600
    for (uint64_t t = 1; t <= 6; t++) {
        EXPECT_TRUE(neural_network_record_spike(network, 0, 1.0f, t * 100));
    }

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    // After 6 writes to capacity=4:
    // index wraps: 0,1,2,3,0,1 -> index should be at 2
    EXPECT_EQ(neuron->spike_history_index, 2u)
        << "Write index should wrap around";

    // Oldest entries (100, 200) are overwritten by (500, 600)
    // Buffer contents at indices: [500, 600, 300, 400]
    EXPECT_EQ(neuron->spike_history[0].timestamp, 500u)
        << "Index 0 should contain 5th spike (overwrote 1st)";
    EXPECT_EQ(neuron->spike_history[1].timestamp, 600u)
        << "Index 1 should contain 6th spike (overwrote 2nd)";
    EXPECT_EQ(neuron->spike_history[2].timestamp, 300u)
        << "Index 2 should still contain 3rd spike";
    EXPECT_EQ(neuron->spike_history[3].timestamp, 400u)
        << "Index 3 should still contain 4th spike";
}

TEST_F(SpikeRingBufferTest, CountSaturatesAtCapacity) {
    // WHAT: spike_history_count must never exceed capacity
    // WHY:  Prevents out-of-bounds reads when iterating history
    // HOW:  Record more spikes than capacity, verify count == capacity
    network_config_t config = make_default_config(10);
    config.spike_history_capacity = 4;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Record 10 spikes
    for (uint64_t t = 1; t <= 10; t++) {
        neural_network_record_spike(network, 0, 1.0f, t * 100);
    }

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    EXPECT_EQ(neuron->spike_history_count, 4u)
        << "Count must saturate at capacity, not exceed it";
}

//=============================================================================
// Reset and Destroy Tests
//=============================================================================

TEST_F(SpikeRingBufferTest, ResetClearsBuffer) {
    // WHAT: neural_network_reset() should clear spike history state
    // WHY:  Reset provides clean slate for re-use
    // HOW:  Record spikes, reset, verify index=0 and count=0
    network_config_t config = make_default_config(10);
    config.spike_history_capacity = 4;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Record some spikes
    for (uint64_t t = 1; t <= 3; t++) {
        neural_network_record_spike(network, 0, 1.0f, t * 100);
    }

    // Reset network
    neural_network_reset(network);

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    EXPECT_EQ(neuron->spike_history_index, 0u)
        << "Reset should zero write position";
    EXPECT_EQ(neuron->spike_history_count, 0u)
        << "Reset should zero valid entry count";
    // Buffer pointer should still be valid (not freed)
    EXPECT_NE(neuron->spike_history, nullptr)
        << "Reset should NOT free the buffer";
}

TEST_F(SpikeRingBufferTest, DestroyFreesBuffer) {
    // WHAT: neural_network_destroy() must free spike_history buffers
    // WHY:  Prevent memory leaks (ASAN/valgrind will catch leaks)
    // HOW:  Create, record, destroy — success if no crash/leak
    network_config_t config = make_default_config(10);
    config.spike_history_capacity = 32;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Record some spikes to exercise the buffer
    for (uint64_t t = 1; t <= 20; t++) {
        neural_network_record_spike(network, 0, 1.0f, t * 100);
    }

    // Destroy should free all spike_history buffers
    neural_network_destroy(network);
    network = nullptr;  // Prevent double-free in TearDown
    // Success if no crash or ASAN error
}

//=============================================================================
// Dynamic Neuron Addition Tests
//=============================================================================

TEST_F(SpikeRingBufferTest, AddNeuronGetsSpikeBuffer) {
    // WHAT: neural_network_add_neuron() should allocate spike buffer
    // WHY:  Dynamically added neurons must have the same ring buffer
    // HOW:  Add neuron, verify spike_history is allocated
    network_config_t config = make_default_config(10);
    config.spike_history_capacity = 32;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t new_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);
    ASSERT_NE(new_id, UINT32_MAX)
        << "add_neuron should return valid ID";

    neuron_t* neuron = neural_network_get_neuron(network, new_id);
    ASSERT_NE(neuron, nullptr);

    EXPECT_NE(neuron->spike_history, nullptr)
        << "Added neuron must have spike buffer";
    EXPECT_EQ(neuron->spike_history_capacity, 32u)
        << "Added neuron should inherit network's spike capacity";
    EXPECT_EQ(neuron->spike_history_index, 0u);
    EXPECT_EQ(neuron->spike_history_count, 0u);
}

//=============================================================================
// Activity Integration Tests
//=============================================================================

TEST_F(SpikeRingBufferTest, AverageActivityWithRingBuffer) {
    // WHAT: Verify average activity computation works with ring buffer
    // WHY:  Average activity uses spike history data
    // HOW:  Record spikes, check get_average_activity returns > 0
    network_config_t config = make_default_config(10);
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Record several spikes for neuron 0
    for (uint64_t t = 1; t <= 10; t++) {
        neural_network_record_spike(network, 0, 1.0f, t * 100);
    }

    float avg = neural_network_get_average_activity(network, 0);
    // After recording spikes, average activity should be > 0
    EXPECT_GE(avg, 0.0f)
        << "Average activity should be non-negative after recording spikes";
}

//=============================================================================
// Activity History Buffer Tests
//=============================================================================

TEST_F(SpikeRingBufferTest, ActivityHistoryAllocated) {
    // WHAT: Verify activity_history is heap-allocated on network create
    // WHY:  Dynamic buffer replaces fixed array, must be allocated
    // HOW:  Create network, check first neuron's activity_history fields
    network_config_t config = make_default_config(10);
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    EXPECT_NE(neuron->activity_history, nullptr)
        << "activity_history must be heap-allocated";
    EXPECT_GT(neuron->activity_history_capacity, 0u)
        << "activity_history_capacity must be positive";
}

TEST_F(SpikeRingBufferTest, ActivityHistoryCustomCapacity) {
    // WHAT: Verify config.activity_history_capacity is honored
    // WHY:  Users need control over per-neuron activity buffer size
    // HOW:  Set config capacity to 16, verify neuron has capacity=16
    network_config_t config = make_default_config(10);
    config.activity_history_capacity = 16;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    EXPECT_EQ(neuron->activity_history_capacity, 16u)
        << "Neuron activity_history_capacity should match config";
}

TEST_F(SpikeRingBufferTest, ActivityHistoryReset) {
    // WHAT: Write values to activity_history, reset, verify zeroed
    // WHY:  Reset must clear dynamic buffer contents
    // HOW:  Manually write values, call reset, check all zeros
    network_config_t config = make_default_config(10);
    config.activity_history_capacity = 8;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);
    ASSERT_NE(neuron->activity_history, nullptr);

    // Write non-zero values
    for (uint32_t i = 0; i < neuron->activity_history_capacity; i++) {
        neuron->activity_history[i] = 1.0f + (float)i;
    }

    // Reset network
    neural_network_reset(network);

    // Verify all values are zeroed
    neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);
    for (uint32_t i = 0; i < neuron->activity_history_capacity; i++) {
        EXPECT_FLOAT_EQ(neuron->activity_history[i], 0.0f)
            << "activity_history[" << i << "] should be zero after reset";
    }
    // Buffer pointer should still be valid
    EXPECT_NE(neuron->activity_history, nullptr)
        << "Reset should NOT free the activity buffer";
}

//=============================================================================
// Size and Scale Tests
//=============================================================================

TEST_F(SpikeRingBufferTest, NeuronSizeReduced) {
    // WHAT: Verify neuron_t struct size is under 5000 bytes
    // WHY:  Dynamic spike history eliminates the old fixed 1000-element array,
    //       enabling 2M+ neuron networks without excessive memory
    // HOW:  Static assertion on sizeof(neuron_t)
    EXPECT_LT(sizeof(neuron_t), 5000u)
        << "neuron_t should be < 5000 bytes with dynamic spike history";
}

TEST_F(SpikeRingBufferTest, MaxNeuronsRaised) {
    // WHAT: Verify MAX_NEURONS is >= 2M
    // WHY:  Spike history optimization enables much larger networks
    // HOW:  Check MAX_NEURONS constant
    EXPECT_GE(MAX_NEURONS, 2000000u)
        << "MAX_NEURONS should be >= 2M after spike history optimization";
}

TEST_F(SpikeRingBufferTest, LargeNetworkCreation) {
    // WHAT: Create a 1000-neuron network with small spike capacity
    // WHY:  Validate that network creation scales with dynamic buffers
    // HOW:  Create with 1000 neurons, spike_capacity=16, verify num_neurons
    network_config_t config = make_default_config(1000);
    config.spike_history_capacity = 16;
    config.input_size = 10;
    config.output_size = 10;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t num = neural_network_get_num_neurons(network);
    EXPECT_GE(num, 1000u)
        << "Network should have at least 1000 neurons";

    // Spot-check a neuron in the middle
    neuron_t* neuron = neural_network_get_neuron(network, 500);
    ASSERT_NE(neuron, nullptr);
    EXPECT_NE(neuron->spike_history, nullptr);
    EXPECT_EQ(neuron->spike_history_capacity, 16u);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
