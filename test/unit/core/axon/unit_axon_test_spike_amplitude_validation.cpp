/**
 * @file unit_axon_test_spike_amplitude_validation.cpp
 * @brief Unit tests for spike amplitude NaN/Inf validation in axon module
 *
 * WHAT: Comprehensive tests for axon spike amplitude validation
 * WHY:  Ensure corrupted spike amplitudes are rejected at axon queue boundary
 * HOW:  GoogleTest framework with comprehensive NaN/Inf coverage
 *
 * TEST COVERAGE:
 * - axon_spike_queue_push() - NaN/Inf rejection at lines 786-794
 * - Valid spike amplitudes
 * - Invalid spike amplitudes (NaN, +Inf, -Inf)
 * - Edge cases and boundary conditions
 * - Queue behavior with rejected spikes
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

// Headers have their own extern "C" guards
#include "core/axon/nimcp_axon.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class AxonSpikeAmplitudeValidationTest : public ::testing::Test {
protected:
    axon_spike_queue_t* queue;
    static constexpr uint32_t QUEUE_CAPACITY = 100;

    void SetUp() override {
        // Initialize logging with low verbosity
        nimcp_logging_set_level(NIMCP_LOG_WARN);

        // Create spike queue
        queue = axon_spike_queue_create(QUEUE_CAPACITY);
        ASSERT_NE(queue, nullptr);
    }

    void TearDown() override {
        if (queue) {
            axon_spike_queue_destroy(queue);
            queue = nullptr;
        }
    }

    // Helper: Create valid spike event
    axon_spike_event_t CreateValidSpike(uint32_t axon_id = 1,
                                        uint64_t time = 1000,
                                        float amplitude = 1.0f) {
        axon_spike_event_t spike;
        spike.axon_id = axon_id;
        spike.initiation_time = time;
        spike.arrival_time = time + 100;
        spike.amplitude = amplitude;
        spike.source_neuron_id = 100;
        spike.target_synapse_id = 200;
        return spike;
    }

    // Helper: Create positive infinity
    float PosInf() const {
        return std::numeric_limits<float>::infinity();
    }

    // Helper: Create negative infinity
    float NegInf() const {
        return -std::numeric_limits<float>::infinity();
    }

    // Helper: Create NaN
    float NaN() const {
        return std::numeric_limits<float>::quiet_NaN();
    }

    // Helper: Get queue size
    uint32_t GetQueueSize() const {
        return axon_spike_queue_size(queue);
    }
};

//=============================================================================
// VALID AMPLITUDE TESTS
//=============================================================================

TEST_F(AxonSpikeAmplitudeValidationTest, ValidAmplitude_Positive) {
    // WHAT: Push spike with normal positive amplitude
    // WHY:  Should succeed
    axon_spike_event_t spike = CreateValidSpike(1, 1000, 1.0f);

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, ValidAmplitude_Zero) {
    // WHAT: Push spike with zero amplitude
    // WHY:  Should succeed (technically valid, though unusual)
    axon_spike_event_t spike = CreateValidSpike(1, 1000, 0.0f);

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, ValidAmplitude_Negative) {
    // WHAT: Push spike with negative amplitude (inhibitory)
    // WHY:  Should succeed (some models use negative for inhibition)
    axon_spike_event_t spike = CreateValidSpike(1, 1000, -1.0f);

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, ValidAmplitude_VerySmall) {
    // WHAT: Push spike with very small amplitude
    // WHY:  Should succeed (weak spike)
    axon_spike_event_t spike = CreateValidSpike(1, 1000, 0.001f);

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, ValidAmplitude_VeryLarge) {
    // WHAT: Push spike with large amplitude
    // WHY:  Should succeed (strong spike)
    axon_spike_event_t spike = CreateValidSpike(1, 1000, 100.0f);

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, ValidAmplitude_Fractional) {
    // WHAT: Push spike with fractional amplitude
    // WHY:  Should succeed (attenuated spike)
    axon_spike_event_t spike = CreateValidSpike(1, 1000, 0.73f);

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, ValidAmplitude_MultipleSpikes) {
    // WHAT: Push multiple spikes with valid amplitudes
    // WHY:  Should all succeed
    for (int i = 0; i < 10; i++) {
        axon_spike_event_t spike = CreateValidSpike(i, 1000 + i * 100, 1.0f + i * 0.1f);
        EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 10u);
}

//=============================================================================
// NaN AMPLITUDE TESTS
//=============================================================================

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_NaN) {
    // WHAT: Push spike with NaN amplitude
    // WHY:  Security validation must reject corrupted spike
    axon_spike_event_t spike = CreateValidSpike(1, 1000, NaN());

    EXPECT_FALSE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 0u);  // Queue should remain empty
}

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_NaN_AfterValidSpikes) {
    // WHAT: Push valid spike, then NaN spike
    // WHY:  Valid spike succeeds, NaN spike fails
    axon_spike_event_t spike1 = CreateValidSpike(1, 1000, 1.0f);
    EXPECT_TRUE(axon_spike_queue_push(queue, &spike1));
    EXPECT_EQ(GetQueueSize(), 1u);

    axon_spike_event_t spike2 = CreateValidSpike(2, 2000, NaN());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike2));
    EXPECT_EQ(GetQueueSize(), 1u);  // Size unchanged
}

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_NaN_MultipleAttempts) {
    // WHAT: Try pushing multiple NaN spikes
    // WHY:  All should fail
    for (int i = 0; i < 5; i++) {
        axon_spike_event_t spike = CreateValidSpike(i, 1000 + i * 100, NaN());
        EXPECT_FALSE(axon_spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 0u);  // Queue remains empty
}

//=============================================================================
// POSITIVE INFINITY AMPLITUDE TESTS
//=============================================================================

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_PosInf) {
    // WHAT: Push spike with positive infinity amplitude
    // WHY:  Security validation must reject infinite value
    axon_spike_event_t spike = CreateValidSpike(1, 1000, PosInf());

    EXPECT_FALSE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_PosInf_AfterValidSpikes) {
    // WHAT: Push valid spike, then +Inf spike
    // WHY:  Valid spike succeeds, +Inf spike fails
    axon_spike_event_t spike1 = CreateValidSpike(1, 1000, 1.0f);
    EXPECT_TRUE(axon_spike_queue_push(queue, &spike1));
    EXPECT_EQ(GetQueueSize(), 1u);

    axon_spike_event_t spike2 = CreateValidSpike(2, 2000, PosInf());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike2));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_PosInf_MultipleAttempts) {
    // WHAT: Try pushing multiple +Inf spikes
    // WHY:  All should fail
    for (int i = 0; i < 5; i++) {
        axon_spike_event_t spike = CreateValidSpike(i, 1000 + i * 100, PosInf());
        EXPECT_FALSE(axon_spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 0u);
}

//=============================================================================
// NEGATIVE INFINITY AMPLITUDE TESTS
//=============================================================================

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_NegInf) {
    // WHAT: Push spike with negative infinity amplitude
    // WHY:  Security validation must reject infinite value
    axon_spike_event_t spike = CreateValidSpike(1, 1000, NegInf());

    EXPECT_FALSE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_NegInf_AfterValidSpikes) {
    // WHAT: Push valid spike, then -Inf spike
    // WHY:  Valid spike succeeds, -Inf spike fails
    axon_spike_event_t spike1 = CreateValidSpike(1, 1000, 1.0f);
    EXPECT_TRUE(axon_spike_queue_push(queue, &spike1));
    EXPECT_EQ(GetQueueSize(), 1u);

    axon_spike_event_t spike2 = CreateValidSpike(2, 2000, NegInf());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike2));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_NegInf_MultipleAttempts) {
    // WHAT: Try pushing multiple -Inf spikes
    // WHY:  All should fail
    for (int i = 0; i < 5; i++) {
        axon_spike_event_t spike = CreateValidSpike(i, 1000 + i * 100, NegInf());
        EXPECT_FALSE(axon_spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 0u);
}

//=============================================================================
// MIXED INVALID TESTS
//=============================================================================

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_Mixed_NaN_Inf) {
    // WHAT: Attempt to push mix of NaN and Inf spikes
    // WHY:  All should fail
    axon_spike_event_t spike1 = CreateValidSpike(1, 1000, NaN());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike1));

    axon_spike_event_t spike2 = CreateValidSpike(2, 2000, PosInf());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike2));

    axon_spike_event_t spike3 = CreateValidSpike(3, 3000, NegInf());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike3));

    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, InvalidAmplitude_Interleaved_Valid_Invalid) {
    // WHAT: Interleave valid and invalid spikes
    // WHY:  Only valid spikes should be added
    axon_spike_event_t spike1 = CreateValidSpike(1, 1000, 1.0f);
    EXPECT_TRUE(axon_spike_queue_push(queue, &spike1));

    axon_spike_event_t spike2 = CreateValidSpike(2, 2000, NaN());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike2));

    axon_spike_event_t spike3 = CreateValidSpike(3, 3000, 2.0f);
    EXPECT_TRUE(axon_spike_queue_push(queue, &spike3));

    axon_spike_event_t spike4 = CreateValidSpike(4, 4000, PosInf());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike4));

    axon_spike_event_t spike5 = CreateValidSpike(5, 5000, 3.0f);
    EXPECT_TRUE(axon_spike_queue_push(queue, &spike5));

    EXPECT_EQ(GetQueueSize(), 3u);  // Only 3 valid spikes added
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

TEST_F(AxonSpikeAmplitudeValidationTest, EdgeCase_NullQueue) {
    // WHAT: Try to push to NULL queue
    // WHY:  Should fail gracefully
    axon_spike_event_t spike = CreateValidSpike(1, 1000, 1.0f);

    EXPECT_FALSE(axon_spike_queue_push(nullptr, &spike));
}

TEST_F(AxonSpikeAmplitudeValidationTest, EdgeCase_NullEvent) {
    // WHAT: Try to push NULL event
    // WHY:  Should fail gracefully
    EXPECT_FALSE(axon_spike_queue_push(queue, nullptr));
    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, EdgeCase_BothNull) {
    // WHAT: Both queue and event are NULL
    // WHY:  Should fail gracefully
    EXPECT_FALSE(axon_spike_queue_push(nullptr, nullptr));
}

TEST_F(AxonSpikeAmplitudeValidationTest, EdgeCase_VerySmallDenormalized) {
    // WHAT: Denormalized (subnormal) floating point amplitude
    // WHY:  Should succeed (still valid float)
    axon_spike_event_t spike = CreateValidSpike(1, 1000,
                                                std::numeric_limits<float>::denorm_min());

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, EdgeCase_MaxFiniteFloat) {
    // WHAT: Maximum finite floating point amplitude
    // WHY:  Should succeed (finite and valid)
    axon_spike_event_t spike = CreateValidSpike(1, 1000,
                                                std::numeric_limits<float>::max());

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, EdgeCase_MinFiniteFloat) {
    // WHAT: Minimum (most negative) finite floating point amplitude
    // WHY:  Should succeed (finite and valid)
    axon_spike_event_t spike = CreateValidSpike(1, 1000,
                                                std::numeric_limits<float>::lowest());

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, EdgeCase_Epsilon) {
    // WHAT: Machine epsilon amplitude
    // WHY:  Should succeed (valid small value)
    axon_spike_event_t spike = CreateValidSpike(1, 1000,
                                                std::numeric_limits<float>::epsilon());

    EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

//=============================================================================
// QUEUE BEHAVIOR TESTS
//=============================================================================

TEST_F(AxonSpikeAmplitudeValidationTest, QueueBehavior_RejectDoesNotCorruptQueue) {
    // WHAT: Verify rejected spike doesn't corrupt queue state
    // WHY:  Queue should remain functional after rejection

    // Add valid spike
    axon_spike_event_t spike1 = CreateValidSpike(1, 1000, 1.0f);
    EXPECT_TRUE(axon_spike_queue_push(queue, &spike1));

    // Try to add invalid spike
    axon_spike_event_t spike2 = CreateValidSpike(2, 2000, NaN());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike2));

    // Add another valid spike
    axon_spike_event_t spike3 = CreateValidSpike(3, 3000, 2.0f);
    EXPECT_TRUE(axon_spike_queue_push(queue, &spike3));

    // Should have 2 spikes
    EXPECT_EQ(GetQueueSize(), 2u);

    // Pop and verify first spike
    axon_spike_event_t popped;
    EXPECT_TRUE(axon_spike_queue_pop(queue, 5000, &popped));
    EXPECT_EQ(popped.axon_id, 1u);
    EXPECT_FLOAT_EQ(popped.amplitude, 1.0f);
}

TEST_F(AxonSpikeAmplitudeValidationTest, QueueBehavior_FillToCapacity_NoInvalid) {
    // WHAT: Fill queue to capacity with valid spikes
    // WHY:  Should succeed for all
    for (uint32_t i = 0; i < QUEUE_CAPACITY; i++) {
        axon_spike_event_t spike = CreateValidSpike(i, 1000 + i * 100, 1.0f);
        EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), QUEUE_CAPACITY);
}

TEST_F(AxonSpikeAmplitudeValidationTest, QueueBehavior_FullQueue_RejectInvalid) {
    // WHAT: Fill queue to capacity, then try invalid spike
    // WHY:  Should reject due to invalid amplitude (not capacity)

    // Fill queue
    for (uint32_t i = 0; i < QUEUE_CAPACITY; i++) {
        axon_spike_event_t spike = CreateValidSpike(i, 1000 + i * 100, 1.0f);
        EXPECT_TRUE(axon_spike_queue_push(queue, &spike));
    }

    // Try to add invalid spike
    axon_spike_event_t spike = CreateValidSpike(QUEUE_CAPACITY, 200000, NaN());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike));

    // Size unchanged
    EXPECT_EQ(GetQueueSize(), QUEUE_CAPACITY);
}

TEST_F(AxonSpikeAmplitudeValidationTest, QueueBehavior_PopAfterReject) {
    // WHAT: Add valid, reject invalid, verify pop works correctly
    // WHY:  Queue operations should be unaffected by rejections

    axon_spike_event_t spike1 = CreateValidSpike(1, 1000, 1.5f);
    EXPECT_TRUE(axon_spike_queue_push(queue, &spike1));

    axon_spike_event_t spike2 = CreateValidSpike(2, 2000, NaN());
    EXPECT_FALSE(axon_spike_queue_push(queue, &spike2));

    axon_spike_event_t popped;
    EXPECT_TRUE(axon_spike_queue_pop(queue, 5000, &popped));
    EXPECT_EQ(popped.axon_id, 1u);
    EXPECT_FLOAT_EQ(popped.amplitude, 1.5f);

    EXPECT_EQ(GetQueueSize(), 0u);
}

//=============================================================================
// STRESS TESTS
//=============================================================================

TEST_F(AxonSpikeAmplitudeValidationTest, Stress_ManyInvalidAttempts) {
    // WHAT: Try pushing many invalid spikes
    // WHY:  Verify rejection is consistent and doesn't leak memory
    for (int i = 0; i < 1000; i++) {
        axon_spike_event_t spike = CreateValidSpike(i, 1000 + i, NaN());
        EXPECT_FALSE(axon_spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(AxonSpikeAmplitudeValidationTest, Stress_AlternatingValidInvalid) {
    // WHAT: Alternate between valid and invalid spikes
    // WHY:  Verify filtering works correctly under rapid changes
    uint32_t valid_count = 0;

    for (int i = 0; i < 100; i++) {
        float amplitude = (i % 2 == 0) ? 1.0f : NaN();
        axon_spike_event_t spike = CreateValidSpike(i, 1000 + i * 100, amplitude);

        bool result = axon_spike_queue_push(queue, &spike);
        if (i % 2 == 0) {
            EXPECT_TRUE(result);
            valid_count++;
        } else {
            EXPECT_FALSE(result);
        }
    }

    EXPECT_EQ(GetQueueSize(), valid_count);
    EXPECT_EQ(GetQueueSize(), 50u);  // Half should succeed
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
