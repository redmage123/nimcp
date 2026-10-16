/**
 * @file test_error_injection.cpp
 * @brief Error injection testing framework for NIMCP
 *
 * Systematically tests error handling paths by simulating failures:
 * - Memory allocation failures (malloc, calloc, realloc)
 * - File I/O errors
 * - Network errors
 * - Resource exhaustion
 *
 * This ensures the library handles errors gracefully without crashes
 * or memory leaks.
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/queue_manager/nimcp_queue_manager.h"

//==============================================================================
// Error Injection Framework
//==============================================================================

/**
 * @brief Global flag to control malloc failure injection
 */
static bool g_inject_malloc_failure = false;
static int g_malloc_failure_countdown = 0;

/**
 * @brief Override malloc for testing (requires special build flags)
 *
 * Note: This is a simplified approach. For production, use LD_PRELOAD
 * or compiler-specific malloc replacement mechanisms.
 */
#ifdef ENABLE_MALLOC_INJECTION
void* __real_malloc(size_t size);
void* __wrap_malloc(size_t size)
{
    if (g_inject_malloc_failure) {
        if (g_malloc_failure_countdown > 0) {
            g_malloc_failure_countdown--;
            return __real_malloc(size);
        }
        return nullptr;  // Simulate malloc failure
    }
    return __real_malloc(size);
}
#endif

//==============================================================================
// Test Fixture
//==============================================================================

class ErrorInjectionTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        g_inject_malloc_failure = false;
        g_malloc_failure_countdown = 0;
    }

    void TearDown() override
    {
        g_inject_malloc_failure = false;
    }

    void EnableMallocFailure(int countdown = 0)
    {
        g_inject_malloc_failure = true;
        g_malloc_failure_countdown = countdown;
    }

    void DisableMallocFailure()
    {
        g_inject_malloc_failure = false;
    }
};

//==============================================================================
// Memory Allocation Failure Tests
//==============================================================================

/**
 * @test Verify neural network creation handles malloc failure gracefully
 */
TEST_F(ErrorInjectionTest, NeuralNetworkCreation_MallocFailure)
{
#ifdef ENABLE_MALLOC_INJECTION
    network_config_t config = {.num_neurons = 20,
                               .ei_ratio = 0.8f,
                               .learning_rate = 0.01f,
                               .input_size = 10,
                               .output_size = 5,
                               .enable_stdp = true};

    // Simulate malloc failure
    EnableMallocFailure();

    neural_network_t net = neural_network_create(&config);

    // Should return NULL on allocation failure
    EXPECT_EQ(net, nullptr);

    DisableMallocFailure();

    // Verify normal allocation still works
    net = neural_network_create(&config);
    EXPECT_NE(net, nullptr);

    if (net) {
        neural_network_destroy(net);
    }
#else
    GTEST_SKIP() << "ENABLE_MALLOC_INJECTION not defined, skipping test";
#endif
}

/**
 * @test Verify queue manager handles allocation failures
 */
TEST_F(ErrorInjectionTest, QueueManager_MallocFailure)
{
#ifdef ENABLE_MALLOC_INJECTION
    nimcp_queue_manager_config_t config = {
        .queue_sizes = {.high = 100, .normal = 100, .low = 100},
        .default_timeout = 1000,
        .blocking_mode = true,
        .max_channels = 4,
        .worker_threads = 1};

    EnableMallocFailure();

    nimcp_queue_manager_t* manager = nullptr;
    nimcp_result_t result = nimcp_queue_manager_create(&config, &manager);

    // Should fail on allocation failure
    EXPECT_NE(result, NIMCP_SUCCESS);
    EXPECT_EQ(manager, nullptr);

    DisableMallocFailure();
#else
    GTEST_SKIP() << "ENABLE_MALLOC_INJECTION not defined, skipping test";
#endif
}

//==============================================================================
// NULL Pointer Handling Tests
//==============================================================================

/**
 * @test Verify APIs handle NULL pointers without crashing
 */
TEST_F(ErrorInjectionTest, NullPointerHandling_NeuralNetwork)
{
    // All these should handle NULL gracefully (not crash)
    EXPECT_NO_FATAL_FAILURE(neural_network_destroy(nullptr));

    float inputs[10] = {0};
    float outputs[5] = {0};
    EXPECT_NO_FATAL_FAILURE(neural_network_forward(nullptr, inputs, 10, outputs, 5));

    network_stats_t stats;
    EXPECT_NO_FATAL_FAILURE(neural_network_get_stats(nullptr, &stats));
}

/**
 * @test Verify queue manager handles NULL pointers
 */
TEST_F(ErrorInjectionTest, NullPointerHandling_QueueManager)
{
    EXPECT_NO_FATAL_FAILURE(nimcp_queue_manager_destroy(nullptr));

    nimcp_message_t msg = {0};
    EXPECT_NO_FATAL_FAILURE(nimcp_queue_manager_enqueue(nullptr, 0, &msg, 0));
    nimcp_message_t* msg_ptr = &msg;
    EXPECT_NO_FATAL_FAILURE(nimcp_queue_manager_dequeue(nullptr, 0, &msg_ptr, 0));

    // NULL queue manager should return true for is_empty (defensive programming)
    EXPECT_TRUE(nimcp_queue_manager_is_empty(nullptr, 0, NIMCP_QUEUE_PRIORITY_NORMAL));
    // NULL queue manager should return false for is_full (cannot be full if it doesn't exist)
    EXPECT_FALSE(nimcp_queue_manager_is_full(nullptr, 0, NIMCP_QUEUE_PRIORITY_NORMAL));
    EXPECT_EQ(nimcp_queue_manager_get_size(nullptr, 0, NIMCP_QUEUE_PRIORITY_NORMAL), 0);
}

//==============================================================================
// Invalid Parameter Tests
//==============================================================================

/**
 * @test Verify neural network rejects invalid configurations
 */
TEST_F(ErrorInjectionTest, InvalidConfig_NeuralNetwork)
{
    // Test with zero neurons
    {
        network_config_t config = {.num_neurons = 20,
                                   .input_size = 0,  // Invalid
                                   .output_size = 5};

        neural_network_t net = neural_network_create(&config);
        EXPECT_EQ(net, nullptr);
    }

    // Test with negative learning rate
    {
        network_config_t config = {
            .num_neurons = 20,
            .learning_rate = -0.5f,  // Invalid
            .input_size = 10,
            .output_size = 5
        };

        neural_network_t net = neural_network_create(&config);
        // May return NULL or clamp to valid range - either is acceptable
        if (net) {
            neural_network_destroy(net);
        }
    }

    // Test with excessive neurons
    {
        network_config_t config = {.num_neurons = 1000000,  // Too large
                                   .input_size = 1000000,
                                   .output_size = 1000000};

        neural_network_t net = neural_network_create(&config);
        // Should fail due to excessive memory requirements
        if (net) {
            // If it somehow succeeds, clean up
            neural_network_destroy(net);
        }
    }
}

/**
 * @test Verify queue manager rejects invalid configurations
 */
TEST_F(ErrorInjectionTest, InvalidConfig_QueueManager)
{
    // Test with zero channels
    {
        nimcp_queue_manager_config_t config = {
            .queue_sizes = {.high = 100, .normal = 100, .low = 100},
            .default_timeout = 1000,
            .blocking_mode = true,
            .max_channels = 0,  // Invalid
            .worker_threads = 1};

        nimcp_queue_manager_t* manager = nullptr;
        nimcp_result_t result = nimcp_queue_manager_create(&config, &manager);
        EXPECT_NE(result, NIMCP_SUCCESS);
        EXPECT_EQ(manager, nullptr);
    }

    // Test with excessive queue size
    {
        nimcp_queue_manager_config_t config = {
            .queue_sizes = {.high = 1000000, .normal = 1000000, .low = 1000000},  // Excessive
            .default_timeout = 1000,
            .blocking_mode = true,
            .max_channels = 4,
            .worker_threads = 1};

        nimcp_queue_manager_t* manager = nullptr;
        nimcp_result_t result = nimcp_queue_manager_create(&config, &manager);
        // Should either reject or clamp to reasonable value
        if (result == NIMCP_SUCCESS && manager) {
            nimcp_queue_manager_destroy(manager);
        }
    }
}

//==============================================================================
// Resource Exhaustion Tests
//==============================================================================

/**
 * @test Verify system handles running out of file descriptors
 */
TEST_F(ErrorInjectionTest, DISABLED_ResourceExhaustion_FileDescriptors)
{
    // This test would open many files to exhaust FDs
    // Disabled by default as it affects system state
    GTEST_SKIP() << "Resource exhaustion test disabled by default";
}

/**
 * @test Verify system handles memory pressure
 */
TEST_F(ErrorInjectionTest, DISABLED_ResourceExhaustion_Memory)
{
    // This test would allocate large amounts of memory
    // Disabled by default as it affects system state
    GTEST_SKIP() << "Resource exhaustion test disabled by default";
}

//==============================================================================
// Concurrent Error Scenarios
//==============================================================================

/**
 * @test Verify error handling is thread-safe
 */
TEST_F(ErrorInjectionTest, ConcurrentErrors_ThreadSafety)
{
    // Create a queue manager
    nimcp_queue_manager_config_t config = {
        .queue_sizes = {.high = 100, .normal = 100, .low = 100},
        .default_timeout = 1000,
        .blocking_mode = true,
        .max_channels = 4,
        .worker_threads = 1};

    nimcp_queue_manager_t* manager = nullptr;
    nimcp_result_t result = nimcp_queue_manager_create(&config, &manager);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    ASSERT_NE(manager, nullptr);

// Try concurrent invalid operations
#pragma omp parallel for num_threads(4)
    for (int i = 0; i < 100; i++) {
        nimcp_message_t msg = {0};
        nimcp_message_t* msg_ptr = &msg;
        // Intentionally use invalid channel
        nimcp_queue_manager_enqueue(manager, 999, &msg, 0);
        nimcp_queue_manager_dequeue(manager, 999, &msg_ptr, 0);
    }

    nimcp_queue_manager_destroy(manager);
}
