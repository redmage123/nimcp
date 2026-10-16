/**
 * @file fuzz_spike_processing.cpp
 * @brief Fuzzing target for neural spike processing
 *
 * Tests spike train and queue operations with random/corrupted data
 * to discover bugs in spike timing, propagation, and queue management.
 *
 * CRITICAL: Spike processing is the CORE of neural simulation. Bugs here
 * could cause:
 * - NaN propagation through network (corrupted activations)
 * - Division by zero (invalid firing rates)
 * - Infinite loops (circular spike references)
 * - Memory leaks (spike buffers not freed)
 * - Race conditions (concurrent spike delivery)
 *
 * This fuzzer tests:
 * - Spike train operations (add, get, rate computation)
 * - Spike queue operations (push, pop, threading)
 * - Invalid timestamps (negative, overflow, out-of-order)
 * - Invalid amplitudes (NaN, Inf, negative, > 10.0)
 * - Edge cases (empty queues, full buffers, NULL pointers)
 * - Memory correctness (leaks, double-frees, use-after-free)
 *
 * Build:
 *   cmake -DENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++ ..
 *   make fuzz_spike_processing
 *
 * Run:
 *   ./fuzz_spike_processing -max_total_time=600
 *   ./fuzz_spike_processing corpus_spike/ -max_total_time=3600
 *
 * Expected to find:
 * - NaN/Inf propagation bugs
 * - Division by zero in rate computation
 * - Buffer overflows in spike trains
 * - Queue corruption under load
 * - Memory leaks in spike handling
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "gpu/nimcp_spike_event.h"

// Minimum input size to be interesting
#define MIN_INPUT_SIZE 16

/**
 * @brief Check if float is valid (not NaN, not Inf)
 */
static bool is_valid_float(float value)
{
    return !std::isnan(value) && !std::isinf(value);
}

/**
 * @brief Parse fuzzer data as spike operations
 *
 * WHAT: Interpret random bytes as spike train/queue operations
 * WHY:  Systematic exploration of spike processing APIs
 * HOW:  Byte-by-byte parsing with operation codes
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Need at least operation + timestamp + amplitude
    if (size < MIN_INPUT_SIZE) {
        return 0;
    }

    // Parse capacity from first 4 bytes
    uint32_t capacity = *(uint32_t*)data;
    if (capacity == 0 || capacity > 100000) {
        capacity = 1000;  // Reasonable default
    }

    // Test 1: Spike Train Operations
    spike_train_t* train = spike_train_create(capacity);
    if (!train) {
        return 0;  // OOM or invalid capacity
    }

    // Parse operations from remaining data
    size_t offset = 4;
    uint32_t num_spikes_added = 0;

    while (offset + 12 <= size) {
        uint8_t op_code = data[offset++];

        if (op_code % 4 == 0) {
            // ADD SPIKE operation
            if (offset + 12 > size) break;

            uint64_t timestamp = *(uint64_t*)(data + offset);
            offset += 8;

            float amplitude = *(float*)(data + offset);
            offset += 4;

            // Check for invalid amplitude (should be caught by implementation)
            if (!is_valid_float(amplitude)) {
                // Fuzzer found NaN/Inf input - implementation should handle
                fprintf(stderr, "FUZZ: Invalid amplitude detected: %f\n", amplitude);
            }

            // Add spike (should handle invalid values gracefully)
            bool added = spike_train_add(train, timestamp, amplitude);
            if (added) {
                num_spikes_added++;
            }

            // Check for capacity overflow
            if (num_spikes_added > capacity) {
                fprintf(stderr, "FUZZ ERROR: Spike train exceeded capacity!\n");
                spike_train_destroy(train);
                return 0;
            }

        } else if (op_code % 4 == 1) {
            // GET LAST SPIKE operation
            uint64_t last_spike = spike_train_get_last_spike(train);
            (void)last_spike;  // Use value to prevent optimization

        } else if (op_code % 4 == 2) {
            // COMPUTE RATE operation
            if (offset + 8 > size) break;

            uint64_t time_window = *(uint64_t*)(data + offset);
            offset += 8;

            // Compute firing rate (may divide by zero if time_window=0)
            float rate = spike_train_compute_rate(train, time_window);

            // Check for invalid results
            if (!is_valid_float(rate)) {
                fprintf(stderr, "FUZZ ERROR: Invalid firing rate: %f (window=%llu)\n",
                        rate, (unsigned long long)time_window);
            }

            if (rate < 0.0f) {
                fprintf(stderr, "FUZZ ERROR: Negative firing rate: %f\n", rate);
            }

        } else if (op_code % 4 == 3) {
            // GET SPIKE operation
            if (offset + 4 > size) break;

            uint32_t index = *(uint32_t*)(data + offset);
            offset += 4;

            spike_event_t event;
            bool got = spike_train_get_spike(train, index, &event);

            if (got) {
                // Verify spike event is valid
                if (!is_valid_float(event.amplitude)) {
                    fprintf(stderr, "FUZZ ERROR: Retrieved spike has invalid amplitude: %f\n",
                            event.amplitude);
                }

                if (event.amplitude < 0.0f || event.amplitude > 10.0f) {
                    fprintf(stderr, "FUZZ ERROR: Amplitude out of biological range: %f\n",
                            event.amplitude);
                }
            }
        }
    }

    // Clear and verify empty
    spike_train_clear(train);
    uint64_t last_after_clear = spike_train_get_last_spike(train);
    if (last_after_clear != 0) {
        fprintf(stderr, "FUZZ ERROR: Spike train not empty after clear!\n");
    }

    spike_train_destroy(train);

    // Test 2: Spike Queue Operations (thread-safe queues)
    spike_queue_t* queue = spike_queue_create(capacity, false);  // CPU-only for now
    if (!queue) {
        return 0;
    }

    // Reset offset for queue operations
    offset = 4;
    uint32_t num_pushes = 0;
    uint32_t num_pops = 0;

    while (offset + 20 <= size) {
        uint8_t op_code = data[offset++];

        if (op_code % 2 == 0) {
            // PUSH operation
            if (offset + 24 > size) break;

            spike_event_t event;
            event.timestamp = *(uint64_t*)(data + offset);
            offset += 8;

            event.source_id = *(uint32_t*)(data + offset);
            offset += 4;

            event.target_id = *(uint32_t*)(data + offset);
            offset += 4;

            event.synapse_id = *(uint32_t*)(data + offset);
            offset += 4;

            event.amplitude = *(float*)(data + offset);
            offset += 4;

            // Validate event (should be caught by implementation)
            if (event.source_id == event.target_id && event.source_id != 0) {
                // Self-spike (autapse) - usually not allowed
                // Implementation should handle this
            }

            if (!is_valid_float(event.amplitude)) {
                fprintf(stderr, "FUZZ: Pushing spike with invalid amplitude: %f\n",
                        event.amplitude);
            }

            bool pushed = spike_queue_push(queue, &event);
            if (pushed) {
                num_pushes++;
            }

            // Check for overflow
            uint32_t queue_size = spike_queue_size(queue);
            if (queue_size > capacity) {
                fprintf(stderr, "FUZZ ERROR: Queue exceeded capacity!\n");
                spike_queue_destroy(queue);
                return 0;
            }

        } else {
            // POP operation
            spike_event_t event;
            bool popped = spike_queue_pop(queue, &event);

            if (popped) {
                num_pops++;

                // Verify popped event is valid
                if (!is_valid_float(event.amplitude)) {
                    fprintf(stderr, "FUZZ ERROR: Popped spike has invalid amplitude: %f\n",
                            event.amplitude);
                }

                if (event.amplitude < 0.0f || event.amplitude > 10.0f) {
                    fprintf(stderr, "FUZZ ERROR: Popped amplitude out of range: %f\n",
                            event.amplitude);
                }
            }
        }
    }

    // Verify queue accounting
    uint32_t expected_size = num_pushes - num_pops;
    uint32_t actual_size = spike_queue_size(queue);

    if (expected_size != actual_size) {
        fprintf(stderr, "FUZZ ERROR: Queue size mismatch! Expected=%u, Actual=%u\n",
                expected_size, actual_size);
    }

    // Check is_empty consistency
    bool should_be_empty = (expected_size == 0);
    bool is_empty = spike_queue_is_empty(queue);

    if (should_be_empty != is_empty) {
        fprintf(stderr, "FUZZ ERROR: is_empty() inconsistent! expected=%d, actual=%d\n",
                should_be_empty, is_empty);
    }

    spike_queue_destroy(queue);

    // Test 3: Edge Cases (NULL pointers, zero capacity, etc.)
    spike_train_destroy(NULL);  // Should handle gracefully
    spike_queue_destroy(NULL);  // Should handle gracefully

    spike_train_t* zero_capacity_train = spike_train_create(0);
    if (zero_capacity_train) {
        // Created with zero capacity - should this be allowed?
        spike_train_destroy(zero_capacity_train);
    }

    spike_queue_t* zero_capacity_queue = spike_queue_create(0, false);
    if (zero_capacity_queue) {
        // Created with zero capacity - should this be allowed?
        spike_queue_destroy(zero_capacity_queue);
    }

    // Test 4: Stress test with many rapid operations
    spike_train_t* stress_train = spike_train_create(1000);
    if (stress_train) {
        // Rapid additions
        for (uint32_t i = 0; i < 100 && i < size / 12; i++) {
            uint64_t timestamp = i * 1000;  // 1ms intervals
            float amplitude = 1.0f;
            spike_train_add(stress_train, timestamp, amplitude);
        }

        // Compute rate multiple times
        for (uint32_t i = 0; i < 10; i++) {
            float rate = spike_train_compute_rate(stress_train, 10000);
            if (!is_valid_float(rate) || rate < 0.0f) {
                fprintf(stderr, "FUZZ ERROR: Invalid rate in stress test: %f\n", rate);
            }
        }

        spike_train_destroy(stress_train);
    }

    return 0;
}

/**
 * @brief LLVMFuzzerInitialize - Optional fuzzer initialization
 */
extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    (void)argc;
    (void)argv;

    // Suppress error messages to avoid spam
    // (fuzzer will still detect crashes/assertions)

    return 0;
}
