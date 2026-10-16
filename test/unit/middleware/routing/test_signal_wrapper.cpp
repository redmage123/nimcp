//=============================================================================
// test_signal_wrapper.cpp - Signal Wrapper Unit Tests
//=============================================================================
/**
 * @file test_signal_wrapper.cpp
 * @brief Comprehensive tests for CoW-based signal wrapper
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_signal_wrapper.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SignalWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0)
            << "Memory leak: " << stats.current_allocated << " bytes";
    }

    // Helper: Create test signal data
    void create_test_data() {
        num_dests = 8;
        signal_size = 256;

        dest_ids = new uint32_t[num_dests];
        for (uint32_t i = 0; i < num_dests; i++) {
            dest_ids[i] = i * 100;
        }

        signal_data = new float[signal_size];
        for (uint32_t i = 0; i < signal_size; i++) {
            signal_data[i] = static_cast<float>(i) * 0.5f;
        }
    }

    void cleanup_test_data() {
        delete[] dest_ids;
        delete[] signal_data;
    }

    uint32_t* dest_ids = nullptr;
    uint32_t num_dests = 0;
    float* signal_data = nullptr;
    uint32_t signal_size = 0;
};

//=============================================================================
// Creation/Destruction Tests
//=============================================================================

TEST_F(SignalWrapperTest, Create_ValidData_Success) {
    create_test_data();

    signal_wrapper_t wrapper = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    ASSERT_NE(wrapper, nullptr);
    EXPECT_EQ(signal_wrapper_refcount(wrapper), 1);
    EXPECT_FALSE(signal_wrapper_is_shared(wrapper));

    signal_wrapper_release(wrapper);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, Create_NullDestIds_ReturnsNull) {
    create_test_data();

    signal_wrapper_t wrapper = signal_wrapper_create(
        nullptr, num_dests, signal_data, signal_size);

    EXPECT_EQ(wrapper, nullptr);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, Create_NullSignalData_ReturnsNull) {
    create_test_data();

    signal_wrapper_t wrapper = signal_wrapper_create(
        dest_ids, num_dests, nullptr, signal_size);

    EXPECT_EQ(wrapper, nullptr);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, Create_ZeroDestinations_ReturnsNull) {
    create_test_data();

    signal_wrapper_t wrapper = signal_wrapper_create(
        dest_ids, 0, signal_data, signal_size);

    EXPECT_EQ(wrapper, nullptr);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, Release_NullWrapper_NoCrash) {
    signal_wrapper_release(nullptr);
    // Should not crash
}

//=============================================================================
// Reference Counting Tests
//=============================================================================

TEST_F(SignalWrapperTest, Acquire_ValidWrapper_CreatesReference) {
    create_test_data();

    signal_wrapper_t original = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);
    ASSERT_NE(original, nullptr);

    signal_wrapper_t ref = signal_wrapper_acquire(original);
    ASSERT_NE(ref, nullptr);
    EXPECT_NE(ref, original);  // Different wrapper instances

    // Both should be shared now
    EXPECT_TRUE(signal_wrapper_is_shared(original));
    EXPECT_TRUE(signal_wrapper_is_shared(ref));

    signal_wrapper_release(ref);
    signal_wrapper_release(original);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, Acquire_NullWrapper_ReturnsNull) {
    signal_wrapper_t ref = signal_wrapper_acquire(nullptr);
    EXPECT_EQ(ref, nullptr);
}

TEST_F(SignalWrapperTest, MultipleAcquires_IncrementsRefcount) {
    create_test_data();

    signal_wrapper_t original = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    std::vector<signal_wrapper_t> refs;
    for (int i = 0; i < 10; i++) {
        refs.push_back(signal_wrapper_acquire(original));
        EXPECT_TRUE(signal_wrapper_is_shared(original));
    }

    // Release all references
    for (auto ref : refs) {
        signal_wrapper_release(ref);
    }

    // Original should be private again after all refs released
    EXPECT_FALSE(signal_wrapper_is_shared(original));

    signal_wrapper_release(original);
    cleanup_test_data();
}

//=============================================================================
// Read Operation Tests
//=============================================================================

TEST_F(SignalWrapperTest, ReadDestinations_ValidWrapper_ReturnsCorrectData) {
    create_test_data();

    signal_wrapper_t wrapper = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    uint32_t out_num_dests = 0;
    const uint32_t* read_dests = signal_wrapper_read_destinations(
        wrapper, &out_num_dests);

    ASSERT_NE(read_dests, nullptr);
    EXPECT_EQ(out_num_dests, num_dests);

    for (uint32_t i = 0; i < num_dests; i++) {
        EXPECT_EQ(read_dests[i], dest_ids[i]);
    }

    signal_wrapper_release(wrapper);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, ReadData_ValidWrapper_ReturnsCorrectData) {
    create_test_data();

    signal_wrapper_t wrapper = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    uint32_t out_signal_size = 0;
    const float* read_data = signal_wrapper_read_data(
        wrapper, &out_signal_size);

    ASSERT_NE(read_data, nullptr);
    EXPECT_EQ(out_signal_size, signal_size);

    for (uint32_t i = 0; i < signal_size; i++) {
        EXPECT_FLOAT_EQ(read_data[i], signal_data[i]);
    }

    signal_wrapper_release(wrapper);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, ReadFromReference_SharedData_SameValues) {
    create_test_data();

    signal_wrapper_t original = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);
    signal_wrapper_t ref = signal_wrapper_acquire(original);

    uint32_t size1 = 0, size2 = 0;
    const float* data1 = signal_wrapper_read_data(original, &size1);
    const float* data2 = signal_wrapper_read_data(ref, &size2);

    ASSERT_NE(data1, nullptr);
    ASSERT_NE(data2, nullptr);
    EXPECT_EQ(size1, size2);

    // Should point to same memory (shared)
    EXPECT_EQ(data1, data2);

    // Values should be identical
    for (uint32_t i = 0; i < signal_size; i++) {
        EXPECT_FLOAT_EQ(data1[i], data2[i]);
    }

    signal_wrapper_release(ref);
    signal_wrapper_release(original);
    cleanup_test_data();
}

//=============================================================================
// Write Operation Tests (CoW Trigger)
//=============================================================================

TEST_F(SignalWrapperTest, WriteData_SingleReference_NoCoW) {
    create_test_data();

    signal_wrapper_t wrapper = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    EXPECT_FALSE(signal_wrapper_is_shared(wrapper));

    uint32_t size = 0;
    float* writable = signal_wrapper_write_data(wrapper, &size);

    ASSERT_NE(writable, nullptr);
    EXPECT_EQ(size, signal_size);

    // Modify data
    writable[0] = 999.0f;

    // Still not shared (only one reference)
    EXPECT_FALSE(signal_wrapper_is_shared(wrapper));

    signal_wrapper_release(wrapper);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, WriteData_MultipleReferences_TriggersCoW) {
    create_test_data();

    signal_wrapper_t original = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);
    signal_wrapper_t ref = signal_wrapper_acquire(original);

    EXPECT_TRUE(signal_wrapper_is_shared(original));
    EXPECT_TRUE(signal_wrapper_is_shared(ref));

    // Read original data pointers
    uint32_t size_orig = 0;
    const float* read_orig = signal_wrapper_read_data(original, &size_orig);

    // Write to reference (triggers CoW)
    uint32_t size_ref = 0;
    float* write_ref = signal_wrapper_write_data(ref, &size_ref);

    ASSERT_NE(write_ref, nullptr);
    EXPECT_EQ(size_ref, signal_size);

    // Modify ref data
    write_ref[0] = 999.0f;

    // Original should be unchanged
    EXPECT_FLOAT_EQ(read_orig[0], signal_data[0]);

    // Ref should have new value
    const float* read_ref = signal_wrapper_read_data(ref, nullptr);
    EXPECT_FLOAT_EQ(read_ref[0], 999.0f);

    signal_wrapper_release(ref);
    signal_wrapper_release(original);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, WriteDestinations_MultipleReferences_TriggersCoW) {
    create_test_data();

    signal_wrapper_t original = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);
    signal_wrapper_t ref = signal_wrapper_acquire(original);

    // Read original dest pointers
    uint32_t num_orig = 0;
    const uint32_t* read_orig = signal_wrapper_read_destinations(original, &num_orig);

    // Write to reference (triggers CoW)
    uint32_t num_ref = 0;
    uint32_t* write_ref = signal_wrapper_write_destinations(ref, &num_ref);

    ASSERT_NE(write_ref, nullptr);
    EXPECT_EQ(num_ref, num_dests);

    // Modify ref destinations
    write_ref[0] = 12345;

    // Original should be unchanged
    EXPECT_EQ(read_orig[0], dest_ids[0]);

    // Ref should have new value
    const uint32_t* read_ref = signal_wrapper_read_destinations(ref, nullptr);
    EXPECT_EQ(read_ref[0], 12345u);

    signal_wrapper_release(ref);
    signal_wrapper_release(original);
    cleanup_test_data();
}

//=============================================================================
// Concurrency Tests
//=============================================================================

TEST_F(SignalWrapperTest, Concurrency_ParallelAcquires_ThreadSafe) {
    create_test_data();

    signal_wrapper_t original = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<signal_wrapper_t> refs(num_threads);

    // Parallel acquire
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&, i]() {
            refs[i] = signal_wrapper_acquire(original);
            EXPECT_NE(refs[i], nullptr);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All should be shared
    EXPECT_TRUE(signal_wrapper_is_shared(original));
    for (auto ref : refs) {
        EXPECT_TRUE(signal_wrapper_is_shared(ref));
    }

    // Release all
    for (auto ref : refs) {
        signal_wrapper_release(ref);
    }
    signal_wrapper_release(original);

    cleanup_test_data();
}

TEST_F(SignalWrapperTest, Concurrency_ParallelReads_ThreadSafe) {
    create_test_data();

    signal_wrapper_t wrapper = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 20; i++) {
        threads.emplace_back([&]() {
            uint32_t size = 0;
            const float* data = signal_wrapper_read_data(wrapper, &size);
            if (data && size == signal_size) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, 20);

    signal_wrapper_release(wrapper);
    cleanup_test_data();
}

//=============================================================================
// Memory Leak Tests
//=============================================================================

TEST_F(SignalWrapperTest, MemoryLeak_RepeatedCreateRelease_NoLeaks) {
    nimcp_memory_stats_t start_stats, end_stats;
    nimcp_memory_get_stats(&start_stats);

    for (int i = 0; i < 100; i++) {
        create_test_data();

        signal_wrapper_t wrapper = signal_wrapper_create(
            dest_ids, num_dests, signal_data, signal_size);
        ASSERT_NE(wrapper, nullptr);

        signal_wrapper_release(wrapper);
        cleanup_test_data();
    }

    nimcp_memory_get_stats(&end_stats);
    EXPECT_EQ(end_stats.current_allocated, start_stats.current_allocated);
}

TEST_F(SignalWrapperTest, MemoryLeak_RepeatedAcquireRelease_NoLeaks) {
    create_test_data();
    nimcp_memory_stats_t start_stats, end_stats;

    signal_wrapper_t original = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    nimcp_memory_get_stats(&start_stats);

    for (int i = 0; i < 1000; i++) {
        signal_wrapper_t ref = signal_wrapper_acquire(original);
        ASSERT_NE(ref, nullptr);
        signal_wrapper_release(ref);
    }

    nimcp_memory_get_stats(&end_stats);
    EXPECT_EQ(end_stats.current_allocated, start_stats.current_allocated);

    signal_wrapper_release(original);
    cleanup_test_data();
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(SignalWrapperTest, Performance_AcquireSpeed_Fast) {
    create_test_data();

    signal_wrapper_t original = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    const int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        signal_wrapper_t ref = signal_wrapper_acquire(original);
        signal_wrapper_release(ref);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_ns = static_cast<double>(duration.count()) / iterations;

    // Should be < 10000ns (10μs) per acquire+release cycle (relaxed for system load)
    // (includes malloc for wrapper struct + atomic operations)
    EXPECT_LT(avg_ns, 10000.0) << "Average: " << avg_ns << "ns per acquire+release";

    signal_wrapper_release(original);
    cleanup_test_data();
}

TEST_F(SignalWrapperTest, Performance_ReadSpeed_VeryFast) {
    create_test_data();

    signal_wrapper_t wrapper = signal_wrapper_create(
        dest_ids, num_dests, signal_data, signal_size);

    const int iterations = 100000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        const float* data = signal_wrapper_read_data(wrapper, nullptr);
        (void)data;  // Prevent optimization
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_ns = static_cast<double>(duration.count()) / iterations;

    // Should be < 100ns per read (function call + guards + pointer dereference)
    EXPECT_LT(avg_ns, 100.0) << "Average: " << avg_ns << "ns per read";

    signal_wrapper_release(wrapper);
    cleanup_test_data();
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SignalWrapperTest, EdgeCase_LargeSignal_Success) {
    const uint32_t large_size = 100000;  // 100K floats = 400KB

    uint32_t* large_dests = new uint32_t[10];
    for (int i = 0; i < 10; i++) large_dests[i] = i;

    float* large_data = new float[large_size];
    for (uint32_t i = 0; i < large_size; i++) {
        large_data[i] = static_cast<float>(i);
    }

    signal_wrapper_t wrapper = signal_wrapper_create(
        large_dests, 10, large_data, large_size);

    ASSERT_NE(wrapper, nullptr);

    uint32_t out_size = 0;
    const float* read_data = signal_wrapper_read_data(wrapper, &out_size);
    EXPECT_EQ(out_size, large_size);
    EXPECT_FLOAT_EQ(read_data[0], 0.0f);
    EXPECT_FLOAT_EQ(read_data[large_size - 1], static_cast<float>(large_size - 1));

    signal_wrapper_release(wrapper);
    delete[] large_dests;
    delete[] large_data;
}

TEST_F(SignalWrapperTest, EdgeCase_SingleElementSignal_Success) {
    uint32_t single_dest = 42;
    float single_value = 3.14f;

    signal_wrapper_t wrapper = signal_wrapper_create(
        &single_dest, 1, &single_value, 1);

    ASSERT_NE(wrapper, nullptr);

    uint32_t out_num = 0;
    const uint32_t* read_dest = signal_wrapper_read_destinations(wrapper, &out_num);
    EXPECT_EQ(out_num, 1u);
    EXPECT_EQ(read_dest[0], 42u);

    uint32_t out_size = 0;
    const float* read_data = signal_wrapper_read_data(wrapper, &out_size);
    EXPECT_EQ(out_size, 1u);
    EXPECT_FLOAT_EQ(read_data[0], 3.14f);

    signal_wrapper_release(wrapper);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
