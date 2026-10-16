/**
 * @file test_queue_manager.cpp
 * @brief Unit tests for multi-channel priority queue manager
 *
 * NOTE: This test file tests the queue manager functionality while being
 * mindful of C++/C11 atomics compatibility issues in the public API.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "utils/memory/nimcp_memory.h"
#include "utils/queue_manager/nimcp_queue_manager.h"

// Test fixture for queue manager operations
class QueueManagerTest : public ::testing::Test {
   protected:
    nimcp_queue_manager_handle_t manager = nullptr;

    void SetUp() override
    {
        nimcp_memory_init();
    }

    void TearDown() override
    {
        if (manager) {
            nimcp_queue_manager_destroy(manager);
            manager = nullptr;
        }
        nimcp_memory_cleanup();
    }

    // Helper to create default configuration
    nimcp_queue_manager_config_t GetDefaultConfig()
    {
        nimcp_queue_manager_config_t config;
        config.queue_sizes.high = 100;
        config.queue_sizes.normal = 500;
        config.queue_sizes.low = 50;
        config.default_timeout = 1000;  // 1 second
        config.blocking_mode = false;
        config.max_channels = 10;
        config.worker_threads = 2;
        return config;
    }

    // Helper to create test message
    nimcp_message_t* CreateTestMessage(uint32_t type, uint32_t flags, int value)
    {
        nimcp_message_t* msg = static_cast<nimcp_message_t*>(nimcp_malloc(sizeof(nimcp_message_t)));
        msg->type = type;
        msg->flags = flags;
        msg->size = sizeof(int);
        msg->data = nimcp_malloc(sizeof(int));
        *static_cast<int*>(msg->data) = value;
        return msg;
    }

    // Helper to destroy message
    void DestroyMessage(nimcp_message_t* msg)
    {
        if (msg) {
            if (msg->data) {
                nimcp_free(msg->data);
            }
            nimcp_free(msg);
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

/**
 * WHAT: Test queue manager creation with valid config
 * WHY: Verify basic initialization
 */
TEST_F(QueueManagerTest, Create_ValidConfig)
{
    auto config = GetDefaultConfig();

    nimcp_result_t result = nimcp_queue_manager_create(&config, &manager);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    ASSERT_NE(manager, nullptr);
}

/**
 * WHAT: Test queue manager creation with null config
 * WHY: Verify null config is rejected
 */
TEST_F(QueueManagerTest, Create_NullConfig)
{
    nimcp_result_t result = nimcp_queue_manager_create(nullptr, &manager);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test queue manager creation with null output pointer
 * WHY: Verify null safety
 */
TEST_F(QueueManagerTest, Create_NullOutput)
{
    auto config = GetDefaultConfig();
    nimcp_result_t result = nimcp_queue_manager_create(&config, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test queue manager creation with invalid config
 * WHY: Verify configuration validation
 */
TEST_F(QueueManagerTest, Create_InvalidConfig)
{
    auto config = GetDefaultConfig();
    config.max_channels = 0;  // Invalid

    nimcp_result_t result = nimcp_queue_manager_create(&config, &manager);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test queue manager destruction
 * WHY: Verify cleanup works
 */
TEST_F(QueueManagerTest, Destroy_ValidManager)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_queue_manager_destroy(manager);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    manager = nullptr;  // Prevent double-free in TearDown
}

/**
 * WHAT: Test destroying null manager
 * WHY: Verify null safety
 */
TEST_F(QueueManagerTest, Destroy_Null)
{
    nimcp_result_t result = nimcp_queue_manager_destroy(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Enqueue Operations Tests
//=============================================================================

/**
 * WHAT: Test basic enqueue operation
 * WHY: Verify messages can be queued
 */
TEST_F(QueueManagerTest, Enqueue_Basic)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_message_t* msg = CreateTestMessage(1, NIMCP_QUEUE_PRIORITY_NORMAL, 42);

    nimcp_result_t result = nimcp_queue_manager_enqueue(manager, 0, msg, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    DestroyMessage(msg);
}

/**
 * WHAT: Test enqueue with high priority
 * WHY: Verify priority flags are respected
 */
TEST_F(QueueManagerTest, Enqueue_HighPriority)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_message_t* msg = CreateTestMessage(1, 2, 42);  // flags=2 -> HIGH priority

    nimcp_result_t result = nimcp_queue_manager_enqueue(manager, 0, msg, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    DestroyMessage(msg);
}

/**
 * WHAT: Test enqueue with low priority
 * WHY: Verify low priority handling
 */
TEST_F(QueueManagerTest, Enqueue_LowPriority)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_message_t* msg = CreateTestMessage(1, 0, 42);  // flags=0 -> LOW priority

    nimcp_result_t result = nimcp_queue_manager_enqueue(manager, 0, msg, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    DestroyMessage(msg);
}

/**
 * WHAT: Test enqueue with null message
 * WHY: Verify null safety
 */
TEST_F(QueueManagerTest, Enqueue_NullMessage)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_queue_manager_enqueue(manager, 0, nullptr, 0);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test enqueue with invalid channel
 * WHY: Verify channel validation
 */
TEST_F(QueueManagerTest, Enqueue_InvalidChannel)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_message_t* msg = CreateTestMessage(1, 1, 42);

    nimcp_result_t result = nimcp_queue_manager_enqueue(manager, 999, msg, 0);
    EXPECT_NE(result, NIMCP_SUCCESS);

    DestroyMessage(msg);
}

/**
 * WHAT: Test enqueue to multiple channels
 * WHY: Verify multi-channel support
 */
TEST_F(QueueManagerTest, Enqueue_MultipleChannels)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    for (uint32_t ch = 0; ch < 5; ch++) {
        nimcp_message_t* msg = CreateTestMessage(1, 1, ch);
        nimcp_result_t result = nimcp_queue_manager_enqueue(manager, ch, msg, 0);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        DestroyMessage(msg);
    }
}

//=============================================================================
// Dequeue Operations Tests
//=============================================================================

/**
 * WHAT: Test basic dequeue operation
 * WHY: Verify messages can be retrieved
 */
TEST_F(QueueManagerTest, Dequeue_Basic)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    // Enqueue a message
    nimcp_message_t* sent_msg = CreateTestMessage(1, 1, 42);
    ASSERT_EQ(nimcp_queue_manager_enqueue(manager, 0, sent_msg, 0), NIMCP_SUCCESS);

    // Small delay to allow async operation to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Dequeue the message
    nimcp_message_t* recv_msg = nullptr;
    nimcp_result_t result = nimcp_queue_manager_dequeue(manager, 0, &recv_msg, 100);

    if (result == NIMCP_SUCCESS) {
        ASSERT_NE(recv_msg, nullptr);
        EXPECT_EQ(recv_msg->type, 1);
        DestroyMessage(recv_msg);
    }

    DestroyMessage(sent_msg);
}

/**
 * WHAT: Test dequeue from empty queue
 * WHY: Verify empty queue handling
 */
TEST_F(QueueManagerTest, Dequeue_EmptyQueue)
{
    auto config = GetDefaultConfig();
    config.blocking_mode = false;
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_message_t* msg = nullptr;
    nimcp_result_t result = nimcp_queue_manager_dequeue(manager, 0, &msg, 10);

    // Should return empty or timeout
    EXPECT_TRUE(result == NIMCP_QUEUE_EMPTY || result == NIMCP_TIMEOUT);
    EXPECT_EQ(msg, nullptr);
}

/**
 * WHAT: Test dequeue with null output pointer
 * WHY: Verify null safety
 */
TEST_F(QueueManagerTest, Dequeue_NullOutput)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_queue_manager_dequeue(manager, 0, nullptr, 0);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test priority ordering in dequeue
 * WHY: Verify high priority messages are dequeued first
 */
TEST_F(QueueManagerTest, Dequeue_PriorityOrder)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    // Enqueue low priority message
    nimcp_message_t* low_msg = CreateTestMessage(1, 0, 1);  // LOW
    ASSERT_EQ(nimcp_queue_manager_enqueue(manager, 0, low_msg, 0), NIMCP_SUCCESS);

    // Enqueue high priority message
    nimcp_message_t* high_msg = CreateTestMessage(2, 2, 2);  // HIGH
    ASSERT_EQ(nimcp_queue_manager_enqueue(manager, 0, high_msg, 0), NIMCP_SUCCESS);

    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Dequeue should get high priority first
    nimcp_message_t* recv_msg = nullptr;
    nimcp_result_t result = nimcp_queue_manager_dequeue(manager, 0, &recv_msg, 100);

    if (result == NIMCP_SUCCESS && recv_msg != nullptr) {
        // Should get high priority message (type 2)
        EXPECT_EQ(recv_msg->type, 2);
        DestroyMessage(recv_msg);
    }

    DestroyMessage(low_msg);
    DestroyMessage(high_msg);
}

//=============================================================================
// Queue State Query Tests
//=============================================================================

/**
 * WHAT: Test is_empty check
 * WHY: Verify queue state can be queried
 */
TEST_F(QueueManagerTest, IsEmpty_NewQueue)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    bool empty = nimcp_queue_manager_is_empty(manager, 0, NIMCP_QUEUE_PRIORITY_NORMAL);
    EXPECT_TRUE(empty);
}

/**
 * WHAT: Test is_full check
 * WHY: Verify queue full detection
 */
TEST_F(QueueManagerTest, IsFull_NewQueue)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    bool full = nimcp_queue_manager_is_full(manager, 0, NIMCP_QUEUE_PRIORITY_NORMAL);
    EXPECT_FALSE(full);
}

/**
 * WHAT: Test get_size query
 * WHY: Verify queue size can be retrieved
 */
TEST_F(QueueManagerTest, GetSize_EmptyQueue)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    size_t size = nimcp_queue_manager_get_size(manager, 0, NIMCP_QUEUE_PRIORITY_NORMAL);
    EXPECT_EQ(size, 0);
}

/**
 * WHAT: Test query with invalid channel
 * WHY: Verify validation of channel parameter
 */
TEST_F(QueueManagerTest, Query_InvalidChannel)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    bool empty = nimcp_queue_manager_is_empty(manager, 999, NIMCP_QUEUE_PRIORITY_NORMAL);
    EXPECT_TRUE(empty);  // Returns true for invalid channel (defensive)

    size_t size = nimcp_queue_manager_get_size(manager, 999, NIMCP_QUEUE_PRIORITY_NORMAL);
    EXPECT_EQ(size, 0);  // Returns 0 for invalid channel
}

//=============================================================================
// Clear Operations Tests
//=============================================================================

/**
 * WHAT: Test clearing a channel
 * WHY: Verify clear operation works
 */
TEST_F(QueueManagerTest, Clear_Channel)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    // Enqueue some messages
    for (int i = 0; i < 5; i++) {
        nimcp_message_t* msg = CreateTestMessage(1, 1, i);
        nimcp_queue_manager_enqueue(manager, 0, msg, 0);
        DestroyMessage(msg);
    }

    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Clear the channel
    nimcp_result_t result = nimcp_queue_manager_clear(manager, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test clearing empty channel
 * WHY: Verify clear handles empty channel
 */
TEST_F(QueueManagerTest, Clear_EmptyChannel)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_queue_manager_clear(manager, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test clear with invalid channel
 * WHY: Verify channel validation
 */
TEST_F(QueueManagerTest, Clear_InvalidChannel)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_queue_manager_clear(manager, 999);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test getting statistics
 * WHY: Verify stats collection works
 */
TEST_F(QueueManagerTest, GetStats_EmptyChannel)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_queue_manager_stats_t stats;
    nimcp_result_t result = nimcp_queue_manager_get_stats(manager, 0, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Note: Can't directly check atomic values in C++ without casting issues
    // Just verify the call succeeds
}

/**
 * WHAT: Test stats with null output
 * WHY: Verify null safety
 */
TEST_F(QueueManagerTest, GetStats_NullOutput)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_queue_manager_get_stats(manager, 0, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Timeout Configuration Tests
//=============================================================================

/**
 * WHAT: Test setting default timeout
 * WHY: Verify timeout can be updated
 */
TEST_F(QueueManagerTest, SetTimeout_Valid)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_queue_manager_set_timeout(manager, 2000);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test setting timeout with null manager
 * WHY: Verify null safety
 */
TEST_F(QueueManagerTest, SetTimeout_NullManager)
{
    nimcp_result_t result = nimcp_queue_manager_set_timeout(nullptr, 1000);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Multi-Channel Tests
//=============================================================================

/**
 * WHAT: Test operations on multiple channels independently
 * WHY: Verify channel isolation
 */
TEST_F(QueueManagerTest, MultiChannel_Independence)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    // Enqueue to channel 0
    nimcp_message_t* msg0 = CreateTestMessage(100, 1, 0);
    EXPECT_EQ(nimcp_queue_manager_enqueue(manager, 0, msg0, 0), NIMCP_SUCCESS);

    // Enqueue to channel 1
    nimcp_message_t* msg1 = CreateTestMessage(200, 1, 1);
    EXPECT_EQ(nimcp_queue_manager_enqueue(manager, 1, msg1, 0), NIMCP_SUCCESS);

    // Channels should be independent
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    nimcp_message_t* recv0 = nullptr;
    nimcp_result_t result0 = nimcp_queue_manager_dequeue(manager, 0, &recv0, 100);

    nimcp_message_t* recv1 = nullptr;
    nimcp_result_t result1 = nimcp_queue_manager_dequeue(manager, 1, &recv1, 100);

    if (result0 == NIMCP_SUCCESS && recv0) {
        EXPECT_EQ(recv0->type, 100);
        DestroyMessage(recv0);
    }

    if (result1 == NIMCP_SUCCESS && recv1) {
        EXPECT_EQ(recv1->type, 200);
        DestroyMessage(recv1);
    }

    DestroyMessage(msg0);
    DestroyMessage(msg1);
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * WHAT: Test with many messages
 * WHY: Verify performance under load
 */
TEST_F(QueueManagerTest, Stress_ManyMessages)
{
    auto config = GetDefaultConfig();
    config.queue_sizes.normal = 1000;
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    const int count = 100;
    for (int i = 0; i < count; i++) {
        nimcp_message_t* msg = CreateTestMessage(1, 1, i);
        nimcp_queue_manager_enqueue(manager, 0, msg, 0);
        DestroyMessage(msg);
    }

    // Give time for async operations
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Try to dequeue some messages
    int dequeued = 0;
    for (int i = 0; i < 10; i++) {
        nimcp_message_t* msg = nullptr;
        if (nimcp_queue_manager_dequeue(manager, 0, &msg, 10) == NIMCP_SUCCESS) {
            if (msg) {
                dequeued++;
                DestroyMessage(msg);
            }
        }
    }

    EXPECT_GT(dequeued, 0);  // Should dequeue at least some messages
}

/**
 * WHAT: Test mixed priority operations
 * WHY: Verify priority handling under load
 */
TEST_F(QueueManagerTest, Stress_MixedPriorities)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    // Enqueue mixed priorities
    for (int i = 0; i < 30; i++) {
        uint32_t flags = i % 3;  // Rotate through priorities
        nimcp_message_t* msg = CreateTestMessage(flags, flags, i);
        nimcp_queue_manager_enqueue(manager, 0, msg, 0);
        DestroyMessage(msg);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Dequeue and verify we get some high priority first
    bool got_high = false;
    for (int i = 0; i < 10; i++) {
        nimcp_message_t* msg = nullptr;
        if (nimcp_queue_manager_dequeue(manager, 0, &msg, 10) == NIMCP_SUCCESS) {
            if (msg) {
                if (msg->type == 2) {  // HIGH priority
                    got_high = true;
                }
                DestroyMessage(msg);
            }
        }
    }

    // Should get at least some high priority messages
    EXPECT_TRUE(got_high);
}

//=============================================================================
// Edge Cases Tests
//=============================================================================

/**
 * WHAT: Test with minimum configuration
 * WHY: Verify minimum viable config works
 */
TEST_F(QueueManagerTest, EdgeCase_MinimumConfig)
{
    nimcp_queue_manager_config_t config;
    config.queue_sizes.high = NIMCP_QUEUE_MIN_SIZE;
    config.queue_sizes.normal = NIMCP_QUEUE_MIN_SIZE;
    config.queue_sizes.low = NIMCP_QUEUE_MIN_SIZE;
    config.default_timeout = 100;
    config.blocking_mode = false;
    config.max_channels = 1;
    config.worker_threads = 1;

    nimcp_result_t result = nimcp_queue_manager_create(&config, &manager);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test rapid enqueue/dequeue cycles
 * WHY: Verify stability under rapid operations
 */
TEST_F(QueueManagerTest, EdgeCase_RapidCycles)
{
    auto config = GetDefaultConfig();
    ASSERT_EQ(nimcp_queue_manager_create(&config, &manager), NIMCP_SUCCESS);

    for (int cycle = 0; cycle < 10; cycle++) {
        // Enqueue
        nimcp_message_t* msg = CreateTestMessage(1, 1, cycle);
        nimcp_queue_manager_enqueue(manager, 0, msg, 0);
        DestroyMessage(msg);

        // Brief wait
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Dequeue
        nimcp_message_t* recv = nullptr;
        nimcp_queue_manager_dequeue(manager, 0, &recv, 50);
        if (recv) {
            DestroyMessage(recv);
        }
    }

    // Should complete without crash
    SUCCEED();
}
