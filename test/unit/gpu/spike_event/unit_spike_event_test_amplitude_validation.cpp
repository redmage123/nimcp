/**
 * @file unit_spike_event_test_amplitude_validation.cpp
 * @brief Unit tests for spike amplitude NaN/Inf validation in GPU spike event module
 *
 * WHAT: Comprehensive tests for GPU spike event amplitude validation
 * WHY:  Ensure corrupted spike amplitudes are rejected at GPU spike queue boundary
 * HOW:  GoogleTest framework with comprehensive NaN/Inf coverage
 *
 * TEST COVERAGE:
 * - spike_queue_push() - NaN/Inf rejection at lines 453-461
 * - Valid spike amplitudes
 * - Invalid spike amplitudes (NaN, +Inf, -Inf)
 * - Edge cases and boundary conditions
 * - Lock-free queue behavior with rejected spikes
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "gpu/nimcp_spike_event.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class SpikeEventAmplitudeValidationTest : public ::testing::Test {
protected:
    spike_queue_t* queue;
    static constexpr uint32_t QUEUE_CAPACITY = 128;  // Must be power of 2

    void SetUp() override {
        // Initialize logging with low verbosity
        nimcp_log_set_level(NULL, LOG_LEVEL_WARN);

        // Create spike queue
        queue = spike_queue_create(QUEUE_CAPACITY, false);  // CPU-only for tests
        ASSERT_NE(queue, nullptr);
    }

    void TearDown() override {
        if (queue) {
            spike_queue_destroy(queue);
            queue = nullptr;
        }
    }

    // Helper: Create valid spike event
    spike_event_t CreateValidSpike(uint32_t source_id = 100,
                                   uint32_t target_id = 200,
                                   uint64_t timestamp = 1000,
                                   float amplitude = 1.0f) {
        spike_event_t spike;
        spike.timestamp = timestamp;
        spike.source_id = source_id;
        spike.target_id = target_id;
        spike.synapse_id = 0;
        spike.amplitude = amplitude;
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
        return spike_queue_size(queue);
    }
};

//=============================================================================
// VALID AMPLITUDE TESTS
//=============================================================================

TEST_F(SpikeEventAmplitudeValidationTest, ValidAmplitude_Positive) {
    // WHAT: Push spike with normal positive amplitude
    // WHY:  Should succeed
    spike_event_t spike = CreateValidSpike(100, 200, 1000, 1.0f);

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, ValidAmplitude_Zero) {
    // WHAT: Push spike with zero amplitude
    // WHY:  Should succeed (technically valid, though unusual)
    spike_event_t spike = CreateValidSpike(100, 200, 1000, 0.0f);

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, ValidAmplitude_Negative) {
    // WHAT: Push spike with negative amplitude
    // WHY:  Should succeed (some models allow this)
    spike_event_t spike = CreateValidSpike(100, 200, 1000, -0.5f);

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, ValidAmplitude_BiologicalRange) {
    // WHAT: Push spike with amplitude in biological range [0, 10]
    // WHY:  Should succeed
    spike_event_t spike = CreateValidSpike(100, 200, 1000, 5.0f);

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, ValidAmplitude_VerySmall) {
    // WHAT: Push spike with very small amplitude
    // WHY:  Should succeed (weak spike)
    spike_event_t spike = CreateValidSpike(100, 200, 1000, 0.0001f);

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, ValidAmplitude_LargeButFinite) {
    // WHAT: Push spike with large but finite amplitude
    // WHY:  Should succeed
    spike_event_t spike = CreateValidSpike(100, 200, 1000, 1000.0f);

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, ValidAmplitude_MultipleSpikes) {
    // WHAT: Push multiple spikes with valid amplitudes
    // WHY:  Should all succeed
    for (int i = 0; i < 10; i++) {
        spike_event_t spike = CreateValidSpike(100 + i, 200 + i, 1000 + i * 100,
                                               1.0f + i * 0.1f);
        EXPECT_TRUE(spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 10u);
}

//=============================================================================
// NaN AMPLITUDE TESTS
//=============================================================================

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_NaN) {
    // WHAT: Push spike with NaN amplitude
    // WHY:  Security validation must reject corrupted spike
    spike_event_t spike = CreateValidSpike(100, 200, 1000, NaN());

    EXPECT_FALSE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 0u);  // Queue should remain empty
}

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_NaN_AfterValidSpikes) {
    // WHAT: Push valid spike, then NaN spike
    // WHY:  Valid spike succeeds, NaN spike fails
    spike_event_t spike1 = CreateValidSpike(100, 200, 1000, 1.0f);
    EXPECT_TRUE(spike_queue_push(queue, &spike1));
    EXPECT_EQ(GetQueueSize(), 1u);

    spike_event_t spike2 = CreateValidSpike(101, 201, 2000, NaN());
    EXPECT_FALSE(spike_queue_push(queue, &spike2));
    EXPECT_EQ(GetQueueSize(), 1u);  // Size unchanged
}

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_NaN_MultipleAttempts) {
    // WHAT: Try pushing multiple NaN spikes
    // WHY:  All should fail
    for (int i = 0; i < 5; i++) {
        spike_event_t spike = CreateValidSpike(100 + i, 200 + i, 1000 + i * 100, NaN());
        EXPECT_FALSE(spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 0u);  // Queue remains empty
}

//=============================================================================
// POSITIVE INFINITY AMPLITUDE TESTS
//=============================================================================

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_PosInf) {
    // WHAT: Push spike with positive infinity amplitude
    // WHY:  Security validation must reject infinite value
    spike_event_t spike = CreateValidSpike(100, 200, 1000, PosInf());

    EXPECT_FALSE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_PosInf_AfterValidSpikes) {
    // WHAT: Push valid spike, then +Inf spike
    // WHY:  Valid spike succeeds, +Inf spike fails
    spike_event_t spike1 = CreateValidSpike(100, 200, 1000, 1.0f);
    EXPECT_TRUE(spike_queue_push(queue, &spike1));
    EXPECT_EQ(GetQueueSize(), 1u);

    spike_event_t spike2 = CreateValidSpike(101, 201, 2000, PosInf());
    EXPECT_FALSE(spike_queue_push(queue, &spike2));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_PosInf_MultipleAttempts) {
    // WHAT: Try pushing multiple +Inf spikes
    // WHY:  All should fail
    for (int i = 0; i < 5; i++) {
        spike_event_t spike = CreateValidSpike(100 + i, 200 + i, 1000 + i * 100, PosInf());
        EXPECT_FALSE(spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 0u);
}

//=============================================================================
// NEGATIVE INFINITY AMPLITUDE TESTS
//=============================================================================

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_NegInf) {
    // WHAT: Push spike with negative infinity amplitude
    // WHY:  Security validation must reject infinite value
    spike_event_t spike = CreateValidSpike(100, 200, 1000, NegInf());

    EXPECT_FALSE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_NegInf_AfterValidSpikes) {
    // WHAT: Push valid spike, then -Inf spike
    // WHY:  Valid spike succeeds, -Inf spike fails
    spike_event_t spike1 = CreateValidSpike(100, 200, 1000, 1.0f);
    EXPECT_TRUE(spike_queue_push(queue, &spike1));
    EXPECT_EQ(GetQueueSize(), 1u);

    spike_event_t spike2 = CreateValidSpike(101, 201, 2000, NegInf());
    EXPECT_FALSE(spike_queue_push(queue, &spike2));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_NegInf_MultipleAttempts) {
    // WHAT: Try pushing multiple -Inf spikes
    // WHY:  All should fail
    for (int i = 0; i < 5; i++) {
        spike_event_t spike = CreateValidSpike(100 + i, 200 + i, 1000 + i * 100, NegInf());
        EXPECT_FALSE(spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 0u);
}

//=============================================================================
// MIXED INVALID TESTS
//=============================================================================

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_Mixed_NaN_Inf) {
    // WHAT: Attempt to push mix of NaN and Inf spikes
    // WHY:  All should fail
    spike_event_t spike1 = CreateValidSpike(100, 200, 1000, NaN());
    EXPECT_FALSE(spike_queue_push(queue, &spike1));

    spike_event_t spike2 = CreateValidSpike(101, 201, 2000, PosInf());
    EXPECT_FALSE(spike_queue_push(queue, &spike2));

    spike_event_t spike3 = CreateValidSpike(102, 202, 3000, NegInf());
    EXPECT_FALSE(spike_queue_push(queue, &spike3));

    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(SpikeEventAmplitudeValidationTest, InvalidAmplitude_Interleaved_Valid_Invalid) {
    // WHAT: Interleave valid and invalid spikes
    // WHY:  Only valid spikes should be added
    spike_event_t spike1 = CreateValidSpike(100, 200, 1000, 1.0f);
    EXPECT_TRUE(spike_queue_push(queue, &spike1));

    spike_event_t spike2 = CreateValidSpike(101, 201, 2000, NaN());
    EXPECT_FALSE(spike_queue_push(queue, &spike2));

    spike_event_t spike3 = CreateValidSpike(102, 202, 3000, 2.0f);
    EXPECT_TRUE(spike_queue_push(queue, &spike3));

    spike_event_t spike4 = CreateValidSpike(103, 203, 4000, PosInf());
    EXPECT_FALSE(spike_queue_push(queue, &spike4));

    spike_event_t spike5 = CreateValidSpike(104, 204, 5000, 3.0f);
    EXPECT_TRUE(spike_queue_push(queue, &spike5));

    EXPECT_EQ(GetQueueSize(), 3u);  // Only 3 valid spikes added
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

TEST_F(SpikeEventAmplitudeValidationTest, EdgeCase_NullQueue) {
    // WHAT: Try to push to NULL queue
    // WHY:  Should fail gracefully
    spike_event_t spike = CreateValidSpike(100, 200, 1000, 1.0f);

    EXPECT_FALSE(spike_queue_push(nullptr, &spike));
}

TEST_F(SpikeEventAmplitudeValidationTest, EdgeCase_NullEvent) {
    // WHAT: Try to push NULL event
    // WHY:  Should fail gracefully
    EXPECT_FALSE(spike_queue_push(queue, nullptr));
    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(SpikeEventAmplitudeValidationTest, EdgeCase_BothNull) {
    // WHAT: Both queue and event are NULL
    // WHY:  Should fail gracefully
    EXPECT_FALSE(spike_queue_push(nullptr, nullptr));
}

TEST_F(SpikeEventAmplitudeValidationTest, EdgeCase_VerySmallDenormalized) {
    // WHAT: Denormalized (subnormal) floating point amplitude
    // WHY:  Should succeed (still valid float)
    spike_event_t spike = CreateValidSpike(100, 200, 1000,
                                          std::numeric_limits<float>::denorm_min());

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, EdgeCase_MaxFiniteFloat) {
    // WHAT: Maximum finite floating point amplitude
    // WHY:  Should succeed (finite and valid)
    spike_event_t spike = CreateValidSpike(100, 200, 1000,
                                          std::numeric_limits<float>::max());

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, EdgeCase_MinFiniteFloat) {
    // WHAT: Minimum (most negative) finite floating point amplitude
    // WHY:  Should succeed (finite and valid)
    spike_event_t spike = CreateValidSpike(100, 200, 1000,
                                          std::numeric_limits<float>::lowest());

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

TEST_F(SpikeEventAmplitudeValidationTest, EdgeCase_Epsilon) {
    // WHAT: Machine epsilon amplitude
    // WHY:  Should succeed (valid small value)
    spike_event_t spike = CreateValidSpike(100, 200, 1000,
                                          std::numeric_limits<float>::epsilon());

    EXPECT_TRUE(spike_queue_push(queue, &spike));
    EXPECT_EQ(GetQueueSize(), 1u);
}

//=============================================================================
// LOCK-FREE QUEUE BEHAVIOR TESTS
//=============================================================================

TEST_F(SpikeEventAmplitudeValidationTest, QueueBehavior_RejectDoesNotCorruptQueue) {
    // WHAT: Verify rejected spike doesn't corrupt lock-free queue state
    // WHY:  Queue should remain functional after rejection

    // Add valid spike
    spike_event_t spike1 = CreateValidSpike(100, 200, 1000, 1.0f);
    EXPECT_TRUE(spike_queue_push(queue, &spike1));

    // Try to add invalid spike
    spike_event_t spike2 = CreateValidSpike(101, 201, 2000, NaN());
    EXPECT_FALSE(spike_queue_push(queue, &spike2));

    // Add another valid spike
    spike_event_t spike3 = CreateValidSpike(102, 202, 3000, 2.0f);
    EXPECT_TRUE(spike_queue_push(queue, &spike3));

    // Should have 2 spikes
    EXPECT_EQ(GetQueueSize(), 2u);

    // Pop and verify first spike
    spike_event_t popped;
    EXPECT_TRUE(spike_queue_pop(queue, &popped));
    EXPECT_EQ(popped.source_id, 100u);
    EXPECT_FLOAT_EQ(popped.amplitude, 1.0f);
}

TEST_F(SpikeEventAmplitudeValidationTest, QueueBehavior_FillToCapacity_NoInvalid) {
    // WHAT: Fill queue to capacity with valid spikes
    // WHY:  Should succeed for all
    for (uint32_t i = 0; i < QUEUE_CAPACITY; i++) {
        spike_event_t spike = CreateValidSpike(100 + i, 200 + i, 1000 + i * 100, 1.0f);
        EXPECT_TRUE(spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), QUEUE_CAPACITY);
}

TEST_F(SpikeEventAmplitudeValidationTest, QueueBehavior_FullQueue_RejectInvalid) {
    // WHAT: Fill queue to capacity, then try invalid spike
    // WHY:  Should reject due to invalid amplitude (checked before capacity)

    // Fill queue
    for (uint32_t i = 0; i < QUEUE_CAPACITY; i++) {
        spike_event_t spike = CreateValidSpike(100 + i, 200 + i, 1000 + i * 100, 1.0f);
        EXPECT_TRUE(spike_queue_push(queue, &spike));
    }

    // Try to add invalid spike (should fail due to NaN, not capacity)
    spike_event_t spike = CreateValidSpike(100 + QUEUE_CAPACITY, 200 + QUEUE_CAPACITY,
                                          200000, NaN());
    EXPECT_FALSE(spike_queue_push(queue, &spike));

    // Size unchanged
    EXPECT_EQ(GetQueueSize(), QUEUE_CAPACITY);
}

TEST_F(SpikeEventAmplitudeValidationTest, QueueBehavior_PopAfterReject) {
    // WHAT: Add valid, reject invalid, verify pop works correctly
    // WHY:  Queue operations should be unaffected by rejections

    spike_event_t spike1 = CreateValidSpike(100, 200, 1000, 1.5f);
    EXPECT_TRUE(spike_queue_push(queue, &spike1));

    spike_event_t spike2 = CreateValidSpike(101, 201, 2000, NaN());
    EXPECT_FALSE(spike_queue_push(queue, &spike2));

    spike_event_t popped;
    EXPECT_TRUE(spike_queue_pop(queue, &popped));
    EXPECT_EQ(popped.source_id, 100u);
    EXPECT_FLOAT_EQ(popped.amplitude, 1.5f);

    EXPECT_EQ(GetQueueSize(), 0u);
}

//=============================================================================
// CONCURRENCY TESTS (Lock-Free Queue)
//=============================================================================

TEST_F(SpikeEventAmplitudeValidationTest, Concurrent_ValidSpikes) {
    // WHAT: Push valid spikes from multiple threads
    // WHY:  Verify lock-free queue handles concurrent valid pushes
    const int NUM_THREADS = 4;
    const int SPIKES_PER_THREAD = 10;

    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &success_count, SPIKES_PER_THREAD]() {
            for (int i = 0; i < SPIKES_PER_THREAD; i++) {
                spike_event_t spike = CreateValidSpike(
                    100 + t * 100 + i,
                    200 + t * 100 + i,
                    1000 + t * 1000 + i * 100,
                    1.0f + i * 0.1f
                );
                if (spike_queue_push(queue, &spike)) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All valid spikes should succeed
    EXPECT_EQ(success_count, NUM_THREADS * SPIKES_PER_THREAD);
    EXPECT_EQ(GetQueueSize(), static_cast<uint32_t>(NUM_THREADS * SPIKES_PER_THREAD));
}

TEST_F(SpikeEventAmplitudeValidationTest, Concurrent_InvalidSpikes) {
    // WHAT: Push invalid (NaN) spikes from multiple threads
    // WHY:  Verify lock-free queue rejects all invalid spikes
    const int NUM_THREADS = 4;
    const int SPIKES_PER_THREAD = 10;

    std::vector<std::thread> threads;
    std::atomic<int> failure_count(0);

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &failure_count, SPIKES_PER_THREAD]() {
            for (int i = 0; i < SPIKES_PER_THREAD; i++) {
                spike_event_t spike = CreateValidSpike(
                    100 + t * 100 + i,
                    200 + t * 100 + i,
                    1000 + t * 1000 + i * 100,
                    NaN()
                );
                if (!spike_queue_push(queue, &spike)) {
                    failure_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All invalid spikes should fail
    EXPECT_EQ(failure_count, NUM_THREADS * SPIKES_PER_THREAD);
    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(SpikeEventAmplitudeValidationTest, Concurrent_MixedSpikes) {
    // WHAT: Push mix of valid and invalid spikes from multiple threads
    // WHY:  Verify lock-free queue filters correctly under concurrent load
    const int NUM_THREADS = 4;
    const int SPIKES_PER_THREAD = 20;

    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);
    std::atomic<int> failure_count(0);

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &success_count, &failure_count, SPIKES_PER_THREAD]() {
            for (int i = 0; i < SPIKES_PER_THREAD; i++) {
                // Alternate between valid and invalid
                float amplitude = (i % 2 == 0) ? 1.0f : NaN();
                spike_event_t spike = CreateValidSpike(
                    100 + t * 100 + i,
                    200 + t * 100 + i,
                    1000 + t * 1000 + i * 100,
                    amplitude
                );
                if (spike_queue_push(queue, &spike)) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Half should succeed, half should fail
    int expected_success = NUM_THREADS * SPIKES_PER_THREAD / 2;
    EXPECT_EQ(success_count, expected_success);
    EXPECT_EQ(failure_count, expected_success);
    EXPECT_EQ(GetQueueSize(), static_cast<uint32_t>(expected_success));
}

//=============================================================================
// STRESS TESTS
//=============================================================================

TEST_F(SpikeEventAmplitudeValidationTest, Stress_ManyInvalidAttempts) {
    // WHAT: Try pushing many invalid spikes
    // WHY:  Verify rejection is consistent and doesn't leak memory
    for (int i = 0; i < 1000; i++) {
        spike_event_t spike = CreateValidSpike(100 + i, 200 + i, 1000 + i, NaN());
        EXPECT_FALSE(spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 0u);
}

TEST_F(SpikeEventAmplitudeValidationTest, Stress_AlternatingValidInvalid) {
    // WHAT: Alternate between valid and invalid spikes
    // WHY:  Verify filtering works correctly under rapid changes
    uint32_t valid_count = 0;

    for (int i = 0; i < 100; i++) {
        float amplitude = (i % 2 == 0) ? 1.0f : NaN();
        spike_event_t spike = CreateValidSpike(100 + i, 200 + i, 1000 + i * 100, amplitude);

        bool result = spike_queue_push(queue, &spike);
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

TEST_F(SpikeEventAmplitudeValidationTest, Stress_AllInfinityTypes) {
    // WHAT: Try all types of infinity in rapid succession
    // WHY:  Verify all are correctly rejected
    for (int i = 0; i < 100; i++) {
        float amplitude;
        switch (i % 3) {
            case 0: amplitude = PosInf(); break;
            case 1: amplitude = NegInf(); break;
            case 2: amplitude = NaN(); break;
            default: amplitude = NaN(); break;
        }

        spike_event_t spike = CreateValidSpike(100 + i, 200 + i, 1000 + i * 100, amplitude);
        EXPECT_FALSE(spike_queue_push(queue, &spike));
    }

    EXPECT_EQ(GetQueueSize(), 0u);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
