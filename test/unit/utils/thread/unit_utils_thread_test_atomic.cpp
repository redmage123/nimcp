/**
 * @file unit_utils_thread_test_atomic.cpp
 * @brief Comprehensive unit tests for NIMCP atomic operations wrapper
 *
 * WHAT: Tests for lock-free atomic primitives with memory ordering
 * WHY:  Ensure correctness, thread-safety, and portability of atomic operations
 * HOW:  Google Test framework with multi-threaded stress tests
 *
 * COVERAGE:
 * - All atomic types (int32, int64, uint32, uint64, ptr)
 * - All operations (load, store, fetch_add, fetch_sub, fetch_and, fetch_or, fetch_xor)
 * - Compare-exchange and exchange operations
 * - Memory ordering semantics
 * - Multi-threaded correctness
 * - Convenience macros
 */

#include <gtest/gtest.h>
#include <pthread.h>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdint>

// Headers have their own extern "C" guards
#include "utils/thread/nimcp_atomic.h"

//=============================================================================
// Test Constants
//=============================================================================

static const int NUM_THREADS = 8;
static const int ITERATIONS_PER_THREAD = 10000;

//=============================================================================
// Basic Initialization Tests
//=============================================================================

/**
 * WHAT: Test atomic int32 initialization and basic load
 * WHY:  Verify initialization sets correct value
 */
TEST(AtomicBasicTest, InitAndLoadInt32)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 42);

    int32_t val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 42);

    // Test negative values
    nimcp_atomic_init_i32(&a, -100);
    val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, -100);
}

/**
 * WHAT: Test atomic int64 initialization and basic load
 * WHY:  Verify 64-bit atomics work correctly
 */
TEST(AtomicBasicTest, InitAndLoadInt64)
{
    nimcp_atomic_int64_t a;
    nimcp_atomic_init_i64(&a, 0x123456789ABCDEF0LL);

    int64_t val = nimcp_atomic_load_i64(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 0x123456789ABCDEF0LL);
}

/**
 * WHAT: Test atomic uint32 initialization and basic load
 * WHY:  Verify unsigned 32-bit atomics work correctly
 */
TEST(AtomicBasicTest, InitAndLoadUInt32)
{
    nimcp_atomic_uint32_t a;
    nimcp_atomic_init_u32(&a, 0xDEADBEEF);

    uint32_t val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 0xDEADBEEF);
}

/**
 * WHAT: Test atomic uint64 initialization and basic load
 * WHY:  Verify unsigned 64-bit atomics work correctly
 */
TEST(AtomicBasicTest, InitAndLoadUInt64)
{
    nimcp_atomic_uint64_t a;
    nimcp_atomic_init_u64(&a, 0xFEDCBA9876543210ULL);

    uint64_t val = nimcp_atomic_load_u64(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 0xFEDCBA9876543210ULL);
}

/**
 * WHAT: Test atomic pointer initialization and basic load
 * WHY:  Verify pointer atomics work correctly
 */
TEST(AtomicBasicTest, InitAndLoadPtr)
{
    nimcp_atomic_ptr_t a;
    int dummy = 42;
    void* ptr = &dummy;

    nimcp_atomic_init_ptr(&a, ptr);
    void* val = nimcp_atomic_load_ptr(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, ptr);

    // Test NULL pointer
    nimcp_atomic_init_ptr(&a, nullptr);
    val = nimcp_atomic_load_ptr(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, nullptr);
}

//=============================================================================
// Store/Load Tests
//=============================================================================

/**
 * WHAT: Test atomic store and load operations
 * WHY:  Verify store correctly updates value
 */
TEST(AtomicStoreLoadTest, StoreLoadInt32)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 0);

    nimcp_atomic_store_i32(&a, 123, NIMCP_MEMORY_ORDER_SEQ_CST);
    int32_t val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 123);

    nimcp_atomic_store_i32(&a, -456, NIMCP_MEMORY_ORDER_SEQ_CST);
    val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, -456);
}

/**
 * WHAT: Test atomic store with different memory orderings
 * WHY:  Verify memory ordering parameters are accepted
 */
TEST(AtomicStoreLoadTest, MemoryOrderings)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 0);

    // RELAXED
    nimcp_atomic_store_i32(&a, 1, NIMCP_MEMORY_ORDER_RELAXED);
    int32_t val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_RELAXED);
    EXPECT_EQ(val, 1);

    // RELEASE/ACQUIRE
    nimcp_atomic_store_i32(&a, 2, NIMCP_MEMORY_ORDER_RELEASE);
    val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_ACQUIRE);
    EXPECT_EQ(val, 2);

    // SEQ_CST
    nimcp_atomic_store_i32(&a, 3, NIMCP_MEMORY_ORDER_SEQ_CST);
    val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 3);
}

//=============================================================================
// Fetch-Add Tests
//=============================================================================

/**
 * WHAT: Test atomic fetch-add operation
 * WHY:  Verify addition and return of old value
 */
TEST(AtomicFetchAddTest, FetchAddInt32)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 10);

    int32_t old = nimcp_atomic_fetch_add_i32(&a, 5, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(old, 10);  // Returns old value

    int32_t new_val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 15);  // New value is 10 + 5

    // Test negative addition
    old = nimcp_atomic_fetch_add_i32(&a, -3, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(old, 15);
    new_val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 12);
}

/**
 * WHAT: Test atomic fetch-add with unsigned types
 * WHY:  Verify unsigned arithmetic works correctly
 */
TEST(AtomicFetchAddTest, FetchAddUInt32)
{
    nimcp_atomic_uint32_t a;
    nimcp_atomic_init_u32(&a, 100);

    uint32_t old = nimcp_atomic_fetch_add_u32(&a, 50, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(old, 100);

    uint32_t new_val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 150);
}

/**
 * WHAT: Test atomic fetch-add with 64-bit values
 * WHY:  Verify 64-bit atomic arithmetic
 */
TEST(AtomicFetchAddTest, FetchAddInt64)
{
    nimcp_atomic_int64_t a;
    nimcp_atomic_init_i64(&a, 1000000000LL);

    int64_t old = nimcp_atomic_fetch_add_i64(&a, 2000000000LL, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(old, 1000000000LL);

    int64_t new_val = nimcp_atomic_load_i64(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 3000000000LL);
}

//=============================================================================
// Fetch-Sub Tests
//=============================================================================

/**
 * WHAT: Test atomic fetch-sub operation
 * WHY:  Verify subtraction and return of old value
 */
TEST(AtomicFetchSubTest, FetchSubInt32)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 100);

    int32_t old = nimcp_atomic_fetch_sub_i32(&a, 30, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(old, 100);

    int32_t new_val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 70);
}

/**
 * WHAT: Test atomic fetch-sub with underflow
 * WHY:  Verify unsigned wrap-around behavior
 */
TEST(AtomicFetchSubTest, FetchSubUInt32Underflow)
{
    nimcp_atomic_uint32_t a;
    nimcp_atomic_init_u32(&a, 10);

    uint32_t old = nimcp_atomic_fetch_sub_u32(&a, 20, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(old, 10);

    // Should wrap around (10 - 20 = -10 = 0xFFFFFFF6 in uint32)
    uint32_t new_val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, (uint32_t)-10);
}

//=============================================================================
// Bitwise Fetch Operations Tests
//=============================================================================

/**
 * WHAT: Test atomic fetch-and operation
 * WHY:  Verify bitwise AND clears bits correctly
 */
TEST(AtomicBitwiseTest, FetchAndUInt32)
{
    nimcp_atomic_uint32_t a;
    nimcp_atomic_init_u32(&a, 0xFF);  // 11111111

    uint32_t old = nimcp_atomic_fetch_and_u32(&a, 0x0F, NIMCP_MEMORY_ORDER_SEQ_CST);  // 00001111
    EXPECT_EQ(old, 0xFF);

    uint32_t new_val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 0x0F);  // 00001111
}

/**
 * WHAT: Test atomic fetch-or operation
 * WHY:  Verify bitwise OR sets bits correctly
 */
TEST(AtomicBitwiseTest, FetchOrUInt32)
{
    nimcp_atomic_uint32_t a;
    nimcp_atomic_init_u32(&a, 0x0F);  // 00001111

    uint32_t old = nimcp_atomic_fetch_or_u32(&a, 0xF0, NIMCP_MEMORY_ORDER_SEQ_CST);  // 11110000
    EXPECT_EQ(old, 0x0F);

    uint32_t new_val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 0xFF);  // 11111111
}

/**
 * WHAT: Test atomic fetch-xor operation
 * WHY:  Verify bitwise XOR toggles bits correctly
 */
TEST(AtomicBitwiseTest, FetchXorUInt32)
{
    nimcp_atomic_uint32_t a;
    nimcp_atomic_init_u32(&a, 0xAA);  // 10101010

    uint32_t old = nimcp_atomic_fetch_xor_u32(&a, 0xFF, NIMCP_MEMORY_ORDER_SEQ_CST);  // 11111111
    EXPECT_EQ(old, 0xAA);

    uint32_t new_val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 0x55);  // 01010101
}

//=============================================================================
// Compare-Exchange Tests
//=============================================================================

/**
 * WHAT: Test compare-exchange success case
 * WHY:  Verify CAS succeeds when value matches expected
 */
TEST(AtomicCompareExchangeTest, SuccessCase)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 100);

    int32_t expected = 100;
    bool success = nimcp_atomic_compare_exchange_i32(&a, &expected, 200, NIMCP_MEMORY_ORDER_SEQ_CST);

    EXPECT_TRUE(success);
    EXPECT_EQ(expected, 100);  // Expected unchanged on success

    int32_t new_val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 200);
}

/**
 * WHAT: Test compare-exchange failure case
 * WHY:  Verify CAS fails when value doesn't match and updates expected
 */
TEST(AtomicCompareExchangeTest, FailureCase)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 100);

    int32_t expected = 50;  // Wrong value
    bool success = nimcp_atomic_compare_exchange_i32(&a, &expected, 200, NIMCP_MEMORY_ORDER_SEQ_CST);

    EXPECT_FALSE(success);
    EXPECT_EQ(expected, 100);  // Expected updated to actual value on failure

    int32_t val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 100);  // Value unchanged on failure
}

/**
 * WHAT: Test compare-exchange with pointers
 * WHY:  Verify pointer CAS works correctly
 */
TEST(AtomicCompareExchangeTest, PointerCAS)
{
    nimcp_atomic_ptr_t a;
    int dummy1 = 1, dummy2 = 2;
    void* ptr1 = &dummy1;
    void* ptr2 = &dummy2;

    nimcp_atomic_init_ptr(&a, ptr1);

    void* expected = ptr1;
    bool success = nimcp_atomic_compare_exchange_ptr(&a, &expected, ptr2, NIMCP_MEMORY_ORDER_SEQ_CST);

    EXPECT_TRUE(success);
    void* new_val = nimcp_atomic_load_ptr(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, ptr2);
}

//=============================================================================
// Exchange Tests
//=============================================================================

/**
 * WHAT: Test atomic exchange (unconditional swap)
 * WHY:  Verify exchange returns old value and stores new value
 */
TEST(AtomicExchangeTest, ExchangeInt32)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 42);

    int32_t old = nimcp_atomic_exchange_i32(&a, 99, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(old, 42);

    int32_t new_val = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, 99);
}

/**
 * WHAT: Test atomic pointer exchange
 * WHY:  Verify pointer exchange works correctly
 */
TEST(AtomicExchangeTest, ExchangePtr)
{
    nimcp_atomic_ptr_t a;
    int dummy1 = 1, dummy2 = 2;
    void* ptr1 = &dummy1;
    void* ptr2 = &dummy2;

    nimcp_atomic_init_ptr(&a, ptr1);

    void* old = nimcp_atomic_exchange_ptr(&a, ptr2, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(old, ptr1);

    void* new_val = nimcp_atomic_load_ptr(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(new_val, ptr2);
}

//=============================================================================
// Convenience Macros Tests
//=============================================================================

/**
 * WHAT: Test NIMCP_ATOMIC_INC_I32 macro
 * WHY:  Verify increment macro returns new value
 */
TEST(AtomicMacroTest, IncrementMacros)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 0);

    int32_t new_val = NIMCP_ATOMIC_INC_I32(&a);
    EXPECT_EQ(new_val, 1);

    new_val = NIMCP_ATOMIC_INC_I32(&a);
    EXPECT_EQ(new_val, 2);

    int32_t final = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(final, 2);
}

/**
 * WHAT: Test NIMCP_ATOMIC_DEC_I32 macro
 * WHY:  Verify decrement macro returns new value
 */
TEST(AtomicMacroTest, DecrementMacros)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 10);

    int32_t new_val = NIMCP_ATOMIC_DEC_I32(&a);
    EXPECT_EQ(new_val, 9);

    new_val = NIMCP_ATOMIC_DEC_I32(&a);
    EXPECT_EQ(new_val, 8);
}

/**
 * WHAT: Test bit manipulation macros
 * WHY:  Verify set/clear/toggle/test bit operations
 */
TEST(AtomicMacroTest, BitManipulation)
{
    nimcp_atomic_uint32_t a;
    nimcp_atomic_init_u32(&a, 0);

    // Set bit 3
    NIMCP_ATOMIC_SET_BIT_U32(&a, 3);
    uint32_t val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 0x08);  // Bit 3 set
    EXPECT_TRUE(NIMCP_ATOMIC_TEST_BIT_U32(&a, 3));

    // Set bit 7
    NIMCP_ATOMIC_SET_BIT_U32(&a, 7);
    val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 0x88);  // Bits 3 and 7 set
    EXPECT_TRUE(NIMCP_ATOMIC_TEST_BIT_U32(&a, 7));

    // Clear bit 3
    NIMCP_ATOMIC_CLEAR_BIT_U32(&a, 3);
    val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 0x80);  // Only bit 7 set
    EXPECT_FALSE(NIMCP_ATOMIC_TEST_BIT_U32(&a, 3));

    // Toggle bit 7 (should clear)
    NIMCP_ATOMIC_TOGGLE_BIT_U32(&a, 7);
    val = nimcp_atomic_load_u32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(val, 0x00);
    EXPECT_FALSE(NIMCP_ATOMIC_TEST_BIT_U32(&a, 7));

    // Toggle bit 7 again (should set)
    NIMCP_ATOMIC_TOGGLE_BIT_U32(&a, 7);
    EXPECT_TRUE(NIMCP_ATOMIC_TEST_BIT_U32(&a, 7));
}

//=============================================================================
// Multi-threaded Tests
//=============================================================================

/**
 * WHAT: Test atomic counter with multiple threads
 * WHY:  Verify thread-safety of fetch-add under contention
 */

struct CounterThreadData {
    nimcp_atomic_int32_t* counter;
    int iterations;
};

static void* counter_thread(void* arg)
{
    CounterThreadData* data = (CounterThreadData*)arg;

    for (int i = 0; i < data->iterations; i++) {
        nimcp_atomic_fetch_add_i32(data->counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
    }

    return nullptr;
}

TEST(AtomicMultithreadedTest, ConcurrentIncrement)
{
    nimcp_atomic_int32_t counter;
    nimcp_atomic_init_i32(&counter, 0);

    CounterThreadData data;
    data.counter = &counter;
    data.iterations = ITERATIONS_PER_THREAD;

    pthread_t threads[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], nullptr, counter_thread, &data);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Verify total
    int32_t total = nimcp_atomic_load_i32(&counter, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(total, NUM_THREADS * ITERATIONS_PER_THREAD);
}

/**
 * WHAT: Test compare-exchange under contention
 * WHY:  Verify CAS loop correctness with multiple threads
 */

struct CASThreadData {
    nimcp_atomic_int32_t* counter;
    int iterations;
};

static void* cas_increment_thread(void* arg)
{
    CASThreadData* data = (CASThreadData*)arg;

    for (int i = 0; i < data->iterations; i++) {
        int32_t old, new_val;
        do {
            old = nimcp_atomic_load_i32(data->counter, NIMCP_MEMORY_ORDER_RELAXED);
            new_val = old + 1;
        } while (!nimcp_atomic_compare_exchange_i32(data->counter, &old, new_val,
                                                      NIMCP_MEMORY_ORDER_RELEASE));
    }

    return nullptr;
}

TEST(AtomicMultithreadedTest, CASLoopIncrement)
{
    nimcp_atomic_int32_t counter;
    nimcp_atomic_init_i32(&counter, 0);

    CASThreadData data;
    data.counter = &counter;
    data.iterations = ITERATIONS_PER_THREAD;

    pthread_t threads[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], nullptr, cas_increment_thread, &data);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Verify total
    int32_t total = nimcp_atomic_load_i32(&counter, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(total, NUM_THREADS * ITERATIONS_PER_THREAD);
}

/**
 * WHAT: Test atomic pointer operations under contention
 * WHY:  Verify pointer atomics are thread-safe
 */

struct PtrThreadData {
    nimcp_atomic_ptr_t* ptr;
    void* my_value;
    int iterations;
};

static void* ptr_exchange_thread(void* arg)
{
    PtrThreadData* data = (PtrThreadData*)arg;

    for (int i = 0; i < data->iterations; i++) {
        // Exchange with our value, then exchange back
        void* old = nimcp_atomic_exchange_ptr(data->ptr, data->my_value, NIMCP_MEMORY_ORDER_ACQ_REL);
        nimcp_atomic_exchange_ptr(data->ptr, old, NIMCP_MEMORY_ORDER_ACQ_REL);
    }

    return nullptr;
}

TEST(AtomicMultithreadedTest, ConcurrentPointerExchange)
{
    nimcp_atomic_ptr_t ptr;
    int dummy = 0;
    nimcp_atomic_init_ptr(&ptr, &dummy);

    pthread_t threads[NUM_THREADS];
    PtrThreadData thread_data[NUM_THREADS];
    int thread_values[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_values[i] = i;
        thread_data[i].ptr = &ptr;
        thread_data[i].my_value = &thread_values[i];
        thread_data[i].iterations = 1000;
        pthread_create(&threads[i], nullptr, ptr_exchange_thread, &thread_data[i]);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Verify final pointer is valid (one of our values or the dummy)
    void* final = nimcp_atomic_load_ptr(&ptr, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_NE(final, nullptr);
}

/**
 * WHAT: Test memory fence operation
 * WHY:  Verify fence compiles and executes without error
 */
TEST(AtomicFenceTest, ThreadFence)
{
    // Test all memory orderings
    nimcp_atomic_thread_fence(NIMCP_MEMORY_ORDER_RELAXED);
    nimcp_atomic_thread_fence(NIMCP_MEMORY_ORDER_ACQUIRE);
    nimcp_atomic_thread_fence(NIMCP_MEMORY_ORDER_RELEASE);
    nimcp_atomic_thread_fence(NIMCP_MEMORY_ORDER_ACQ_REL);
    nimcp_atomic_thread_fence(NIMCP_MEMORY_ORDER_SEQ_CST);

    // No crash = success
    SUCCEED();
}

/**
 * WHAT: Test publish-subscribe pattern with acquire-release semantics
 * WHY:  Verify memory ordering prevents data races
 */

struct Message {
    int data;
    bool valid;
};

struct PubSubData {
    Message message;
    nimcp_atomic_int32_t ready;
};

static void* publisher_thread(void* arg)
{
    PubSubData* data = (PubSubData*)arg;

    // Prepare message (non-atomic writes)
    data->message.data = 42;
    data->message.valid = true;

    // Publish with RELEASE (ensures message writes are visible)
    nimcp_atomic_store_i32(&data->ready, 1, NIMCP_MEMORY_ORDER_RELEASE);

    return nullptr;
}

static void* subscriber_thread(void* arg)
{
    PubSubData* data = (PubSubData*)arg;

    // Wait with ACQUIRE (ensures message reads see published writes)
    while (nimcp_atomic_load_i32(&data->ready, NIMCP_MEMORY_ORDER_ACQUIRE) == 0) {
        // Spin wait
    }

    // Message should be valid now
    EXPECT_TRUE(data->message.valid);
    EXPECT_EQ(data->message.data, 42);

    return nullptr;
}

TEST(AtomicMemoryOrderingTest, AcquireReleaseSemantics)
{
    PubSubData data;
    data.message.data = 0;
    data.message.valid = false;
    nimcp_atomic_init_i32(&data.ready, 0);

    pthread_t pub, sub;

    pthread_create(&sub, nullptr, subscriber_thread, &data);
    pthread_create(&pub, nullptr, publisher_thread, &data);

    pthread_join(pub, nullptr);
    pthread_join(sub, nullptr);

    // Test assertions are in subscriber_thread
}

/**
 * WHAT: Stress test with mixed operations
 * WHY:  Verify correctness under heavy contention
 */

struct MixedOpThreadData {
    nimcp_atomic_int32_t* counter;
    nimcp_atomic_uint32_t* flags;
    int iterations;
    int thread_id;
};

static void* mixed_operations_thread(void* arg)
{
    MixedOpThreadData* data = (MixedOpThreadData*)arg;

    for (int i = 0; i < data->iterations; i++) {
        // Increment counter
        nimcp_atomic_fetch_add_i32(data->counter, 1, NIMCP_MEMORY_ORDER_RELAXED);

        // Set a bit in flags
        NIMCP_ATOMIC_SET_BIT_U32(data->flags, data->thread_id % 32);

        // Clear the bit
        NIMCP_ATOMIC_CLEAR_BIT_U32(data->flags, data->thread_id % 32);

        // Decrement counter
        nimcp_atomic_fetch_sub_i32(data->counter, 1, NIMCP_MEMORY_ORDER_RELAXED);
    }

    return nullptr;
}

TEST(AtomicStressTest, MixedOperations)
{
    nimcp_atomic_int32_t counter;
    nimcp_atomic_uint32_t flags;
    nimcp_atomic_init_i32(&counter, 0);
    nimcp_atomic_init_u32(&flags, 0);

    pthread_t threads[NUM_THREADS];
    MixedOpThreadData thread_data[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].counter = &counter;
        thread_data[i].flags = &flags;
        thread_data[i].iterations = ITERATIONS_PER_THREAD;
        thread_data[i].thread_id = i;
        pthread_create(&threads[i], nullptr, mixed_operations_thread, &thread_data[i]);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Counter should be back to 0 (each thread does +1 then -1)
    int32_t final_counter = nimcp_atomic_load_i32(&counter, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(final_counter, 0);

    // Flags should be all cleared
    uint32_t final_flags = nimcp_atomic_load_u32(&flags, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(final_flags, 0);
}

//=============================================================================
// Atomic Bool Tests
//=============================================================================

/**
 * WHAT: Test atomic bool initialization and basic operations
 * WHY:  Verify bool atomics work correctly
 */
TEST(AtomicBoolTest, InitAndLoadBool)
{
    nimcp_atomic_bool_t a;
    nimcp_atomic_init_bool(&a, true);

    bool val = nimcp_atomic_load_bool(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_TRUE(val);

    nimcp_atomic_init_bool(&a, false);
    val = nimcp_atomic_load_bool(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_FALSE(val);
}

/**
 * WHAT: Test atomic bool store and load
 * WHY:  Verify store correctly updates bool value
 */
TEST(AtomicBoolTest, StoreLoadBool)
{
    nimcp_atomic_bool_t a;
    nimcp_atomic_init_bool(&a, false);

    nimcp_atomic_store_bool(&a, true, NIMCP_MEMORY_ORDER_SEQ_CST);
    bool val = nimcp_atomic_load_bool(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_TRUE(val);

    nimcp_atomic_store_bool(&a, false, NIMCP_MEMORY_ORDER_SEQ_CST);
    val = nimcp_atomic_load_bool(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_FALSE(val);
}

/**
 * WHAT: Test atomic bool compare-exchange
 * WHY:  Verify CAS works for bool (test-and-set pattern)
 */
TEST(AtomicBoolTest, CompareExchangeBool)
{
    nimcp_atomic_bool_t a;
    nimcp_atomic_init_bool(&a, false);

    // Try to acquire (false -> true)
    bool expected = false;
    bool success = nimcp_atomic_compare_exchange_bool(&a, &expected, true, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_TRUE(success);
    EXPECT_FALSE(expected);  // Expected unchanged on success

    bool val = nimcp_atomic_load_bool(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_TRUE(val);

    // Try to acquire again (should fail, already true)
    expected = false;
    success = nimcp_atomic_compare_exchange_bool(&a, &expected, true, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_FALSE(success);
    EXPECT_TRUE(expected);  // Expected updated to actual value

    val = nimcp_atomic_load_bool(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_TRUE(val);  // Still true
}

/**
 * WHAT: Test atomic bool exchange
 * WHY:  Verify exchange (test-and-set) pattern
 */
TEST(AtomicBoolTest, ExchangeBool)
{
    nimcp_atomic_bool_t a;
    nimcp_atomic_init_bool(&a, false);

    // Test-and-set
    bool old = nimcp_atomic_exchange_bool(&a, true, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_FALSE(old);  // Was false

    bool val = nimcp_atomic_load_bool(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_TRUE(val);  // Now true

    // Test-and-clear
    old = nimcp_atomic_exchange_bool(&a, false, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_TRUE(old);  // Was true

    val = nimcp_atomic_load_bool(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_FALSE(val);  // Now false
}

/**
 * WHAT: Test atomic bool as spinlock
 * WHY:  Verify bool atomics work for synchronization
 */

struct BoolSpinlockData {
    nimcp_atomic_bool_t* lock;
    int* shared_counter;
    int iterations;
};

static void* bool_spinlock_thread(void* arg)
{
    BoolSpinlockData* data = (BoolSpinlockData*)arg;

    for (int i = 0; i < data->iterations; i++) {
        // Acquire lock
        bool expected;
        do {
            expected = false;
        } while (!nimcp_atomic_compare_exchange_bool(data->lock, &expected, true,
                                                       NIMCP_MEMORY_ORDER_ACQUIRE));

        // Critical section
        (*data->shared_counter)++;

        // Release lock
        nimcp_atomic_store_bool(data->lock, false, NIMCP_MEMORY_ORDER_RELEASE);
    }

    return nullptr;
}

TEST(AtomicBoolTest, BoolAsSpinlock)
{
    nimcp_atomic_bool_t lock;
    nimcp_atomic_init_bool(&lock, false);

    int shared_counter = 0;

    BoolSpinlockData data;
    data.lock = &lock;
    data.shared_counter = &shared_counter;
    data.iterations = 1000;

    pthread_t threads[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], nullptr, bool_spinlock_thread, &data);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Verify counter
    EXPECT_EQ(shared_counter, NUM_THREADS * 1000);

    // Verify lock is released
    bool lock_state = nimcp_atomic_load_bool(&lock, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_FALSE(lock_state);
}

//=============================================================================
// Convenience Function Tests (increment/decrement)
//=============================================================================

/**
 * WHAT: Test nimcp_atomic_increment_i32 function
 * WHY:  Verify increment function returns new value
 */
TEST(AtomicConvenienceFunctionTest, IncrementFunctionsInt32)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 0);

    int32_t new_val = nimcp_atomic_increment_i32(&a);
    EXPECT_EQ(new_val, 1);

    new_val = nimcp_atomic_increment_i32(&a);
    EXPECT_EQ(new_val, 2);

    int32_t final = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(final, 2);
}

/**
 * WHAT: Test nimcp_atomic_decrement_i32 function
 * WHY:  Verify decrement function returns new value
 */
TEST(AtomicConvenienceFunctionTest, DecrementFunctionsInt32)
{
    nimcp_atomic_int32_t a;
    nimcp_atomic_init_i32(&a, 10);

    int32_t new_val = nimcp_atomic_decrement_i32(&a);
    EXPECT_EQ(new_val, 9);

    new_val = nimcp_atomic_decrement_i32(&a);
    EXPECT_EQ(new_val, 8);

    int32_t final = nimcp_atomic_load_i32(&a, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(final, 8);
}

/**
 * WHAT: Test increment/decrement functions for all types
 * WHY:  Verify type-specific functions work correctly
 */
TEST(AtomicConvenienceFunctionTest, AllTypeIncrementDecrement)
{
    // int64
    nimcp_atomic_int64_t i64;
    nimcp_atomic_init_i64(&i64, 100);
    EXPECT_EQ(nimcp_atomic_increment_i64(&i64), 101);
    EXPECT_EQ(nimcp_atomic_decrement_i64(&i64), 100);

    // uint32
    nimcp_atomic_uint32_t u32;
    nimcp_atomic_init_u32(&u32, 50);
    EXPECT_EQ(nimcp_atomic_increment_u32(&u32), 51);
    EXPECT_EQ(nimcp_atomic_decrement_u32(&u32), 50);

    // uint64
    nimcp_atomic_uint64_t u64;
    nimcp_atomic_init_u64(&u64, 1000);
    EXPECT_EQ(nimcp_atomic_increment_u64(&u64), 1001);
    EXPECT_EQ(nimcp_atomic_decrement_u64(&u64), 1000);
}

/**
 * WHAT: Test increment/decrement functions under concurrency
 * WHY:  Verify thread-safety of convenience functions
 */

struct IncrementFunctionThreadData {
    nimcp_atomic_int32_t* counter;
    int iterations;
};

static void* increment_function_thread(void* arg)
{
    IncrementFunctionThreadData* data = (IncrementFunctionThreadData*)arg;

    for (int i = 0; i < data->iterations; i++) {
        nimcp_atomic_increment_i32(data->counter);
    }

    return nullptr;
}

TEST(AtomicConvenienceFunctionTest, ConcurrentIncrementFunctions)
{
    nimcp_atomic_int32_t counter;
    nimcp_atomic_init_i32(&counter, 0);

    IncrementFunctionThreadData data;
    data.counter = &counter;
    data.iterations = ITERATIONS_PER_THREAD;

    pthread_t threads[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], nullptr, increment_function_thread, &data);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    // Verify total
    int32_t total = nimcp_atomic_load_i32(&counter, NIMCP_MEMORY_ORDER_SEQ_CST);
    EXPECT_EQ(total, NUM_THREADS * ITERATIONS_PER_THREAD);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
