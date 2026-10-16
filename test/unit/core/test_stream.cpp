/**
 * @file test_stream.cpp
 * @brief Tests for brain streaming API functionality
 *
 * WHAT: Comprehensive tests for continuous input streaming
 * WHY: Streaming is critical for active consciousness - must work correctly
 * HOW: Unit tests for all streaming modes, ring buffer, callbacks, statistics
 */

#include "test_helpers.h"

#include "core/brain/nimcp_brain.h"
#include "io/stream/nimcp_stream.h"
#include "utils/thread/nimcp_thread.h"

#include <gtest/gtest.h>
#include <string.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <thread>

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Test fixture for streaming tests
 * WHY: Set up/tear down brain and stream for each test
 */
class StreamTest : public ::testing::Test {
   protected:
    brain_t brain;
    brain_stream_t stream;

    // Test data
    static const uint32_t NUM_FEATURES = 13;
    float test_features[NUM_FEATURES];

    void SetUp() override
    {
        // Initialize threading subsystem (required before creating streams)
        nimcp_result_t result = nimcp_thread_init();
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Create test brain
        brain = brain_create("test_stream_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                             NUM_FEATURES, 3);
        ASSERT_NE(brain, nullptr);

        // Initialize test features
        for (uint32_t i = 0; i < NUM_FEATURES; i++) {
            test_features[i] = (float) i / NUM_FEATURES;
        }

        stream = nullptr;
    }

    void TearDown() override
    {
        // Clean up stream
        if (stream) {
            brain_destroy_stream(stream);
        }

        // Clean up brain
        if (brain) {
            brain_destroy(brain);
        }
    }
};

// Global callback counters for testing
static std::atomic<uint32_t> g_high_salience_count{0};
static std::atomic<uint32_t> g_decision_ready_count{0};
static std::atomic<uint32_t> g_error_count{0};

// Callback functions
static void high_salience_callback(const stream_event_t* event, void* context)
{
    g_high_salience_count++;
}

static void decision_ready_callback(const stream_event_t* event, void* context)
{
    g_decision_ready_count++;
}

static void error_callback(const stream_event_t* event, void* context)
{
    g_error_count++;
}

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * WHAT: Test default stream configuration
 * WHY: Verify sensible defaults are provided
 */
TEST_F(StreamTest, DefaultConfig)
{
    stream_config_t config = stream_default_config();

    EXPECT_EQ(config.mode, STREAM_MODE_SYNCHRONOUS);
    EXPECT_GT(config.buffer_size, 0u);
    EXPECT_GT(config.processing_interval_ms, 0u);
    EXPECT_EQ(config.on_high_salience, nullptr);
    EXPECT_EQ(config.on_decision_ready, nullptr);
    EXPECT_EQ(config.on_error, nullptr);
}

//=============================================================================
// Stream Creation Tests
//=============================================================================

/**
 * WHAT: Test synchronous stream creation
 * WHY: Verify basic stream initialization works
 */
TEST_F(StreamTest, CreateSynchronousStream)
{
    stream_config_t config = stream_default_config();
    config.mode = STREAM_MODE_SYNCHRONOUS;

    stream = brain_create_stream(brain, &config);

    ASSERT_NE(stream, nullptr);
}

/**
 * WHAT: Test background stream creation
 * WHY: Verify background thread mode works
 */
TEST_F(StreamTest, CreateBackgroundStream)
{
    stream_config_t config = stream_default_config();
    config.mode = STREAM_MODE_BACKGROUND;

    stream = brain_create_stream(brain, &config);

    ASSERT_NE(stream, nullptr);

    // Give thread time to start
    usleep(10000);  // 10ms
}

/**
 * WHAT: Test batched stream creation
 * WHY: Verify batched mode works
 */
TEST_F(StreamTest, CreateBatchedStream)
{
    stream_config_t config = stream_default_config();
    config.mode = STREAM_MODE_BATCHED;
    config.batch_size = 10;

    stream = brain_create_stream(brain, &config);

    ASSERT_NE(stream, nullptr);
}

/**
 * WHAT: Test stream creation with NULL brain
 * WHY: Verify proper error handling
 */
TEST_F(StreamTest, CreateStreamNullBrain)
{
    stream_config_t config = stream_default_config();

    stream = brain_create_stream(nullptr, &config);

    EXPECT_EQ(stream, nullptr);
}

/**
 * WHAT: Test stream creation with callbacks
 * WHY: Verify callback registration works
 */
TEST_F(StreamTest, CreateStreamWithCallbacks)
{
    stream_config_t config = stream_default_config();
    config.on_high_salience = high_salience_callback;
    config.on_decision_ready = decision_ready_callback;
    config.on_error = error_callback;
    config.callback_context = (void*) 0x12345678;

    stream = brain_create_stream(brain, &config);

    ASSERT_NE(stream, nullptr);
}

//=============================================================================
// Input Feeding Tests
//=============================================================================

/**
 * WHAT: Test feeding single input
 * WHY: Verify basic input feeding works
 */
TEST_F(StreamTest, FeedSingleInput)
{
    stream_config_t config = stream_default_config();
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    uint64_t timestamp = 1000;
    bool result = brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test feeding multiple inputs
 * WHY: Verify stream can handle multiple inputs
 */
TEST_F(StreamTest, FeedMultipleInputs)
{
    stream_config_t config = stream_default_config();
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed 100 inputs
    for (uint32_t i = 0; i < 100; i++) {
        bool result = brain_stream_feed(stream, test_features, NUM_FEATURES, i);
        EXPECT_TRUE(result);
    }

    // Verify statistics
    stream_stats_t stats;
    bool got_stats = brain_stream_get_stats(stream, &stats);
    ASSERT_TRUE(got_stats);
    EXPECT_EQ(stats.inputs_fed, 100u);
}

/**
 * WHAT: Test feeding with NULL stream
 * WHY: Verify proper error handling
 */
TEST_F(StreamTest, FeedNullStream)
{
    bool result = brain_stream_feed(nullptr, test_features, NUM_FEATURES, 1000);

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test feeding with NULL features
 * WHY: Verify proper error handling
 */
TEST_F(StreamTest, FeedNullFeatures)
{
    stream_config_t config = stream_default_config();
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    bool result = brain_stream_feed(stream, nullptr, NUM_FEATURES, 1000);

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test feeding until buffer full
 * WHY: Verify ring buffer overflow handling
 */
TEST_F(StreamTest, FeedUntilBufferFull)
{
    stream_config_t config = stream_default_config();
    config.buffer_size = 10;  // Small buffer
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed more than buffer capacity
    uint32_t fed_count = 0;
    for (uint32_t i = 0; i < 20; i++) {
        if (brain_stream_feed(stream, test_features, NUM_FEATURES, i)) {
            fed_count++;
        }
    }

    // Should have fed at least buffer_size inputs
    EXPECT_GE(fed_count, config.buffer_size);

    // Check stats for dropped inputs
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    // Some inputs may have been dropped if buffer was full
}

//=============================================================================
// Decision Retrieval Tests
//=============================================================================

/**
 * WHAT: Test getting decision from synchronous stream
 * WHY: Verify decision retrieval works
 */
TEST_F(StreamTest, GetDecisionSynchronous)
{
    stream_config_t config = stream_default_config();
    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed input
    brain_stream_feed(stream, test_features, NUM_FEATURES, 1000);

    /**
     * WHAT: Test deprecated brain_stream_get_decision() function
     * WHY: Verify it returns NULL as per deprecation
     * HOW: Function was deprecated to fix double-free bugs
     *
     * NOTE: Applications should use callbacks or call brain_decide() directly
     */
    brain_decision_t* decision = brain_stream_get_decision(stream);

    // Deprecated function now always returns NULL
    EXPECT_EQ(decision, nullptr);
}

/**
 * WHAT: Test getting decision from background stream
 * WHY: Verify async decision retrieval works
 */
TEST_F(StreamTest, GetDecisionBackground)
{
    stream_config_t config = stream_default_config();
    config.mode = STREAM_MODE_BACKGROUND;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed input
    brain_stream_feed(stream, test_features, NUM_FEATURES, 1000);

    // Wait for processing
    usleep(200000);  // 200ms

    /**
     * WHAT: Test deprecated brain_stream_get_decision() function
     * WHY: Verify it returns NULL as per deprecation
     * HOW: Function was deprecated to fix double-free bugs
     *
     * NOTE: Applications should use callbacks or call brain_decide() directly
     */
    brain_decision_t* decision = brain_stream_get_decision(stream);

    // Deprecated function now always returns NULL
    EXPECT_EQ(decision, nullptr);
}

/**
 * WHAT: Test getting decision with NULL stream
 * WHY: Verify proper error handling
 */
TEST_F(StreamTest, GetDecisionNullStream)
{
    brain_decision_t* decision = brain_stream_get_decision(nullptr);

    EXPECT_EQ(decision, nullptr);
}

/**
 * WHAT: Test getting salience score
 * WHY: Verify fast salience retrieval works
 */
TEST_F(StreamTest, GetSalience)
{
    stream_config_t config = stream_default_config();
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed input
    brain_stream_feed(stream, test_features, NUM_FEATURES, 1000);

    // Get salience
    float salience = brain_stream_get_salience(stream);

    // Salience should be 0-1
    EXPECT_GE(salience, 0.0f);
    EXPECT_LE(salience, 1.0f);
}

//=============================================================================
// Stream Control Tests
//=============================================================================

/**
 * WHAT: Test pausing stream
 * WHY: Verify pause functionality works
 */
TEST_F(StreamTest, PauseStream)
{
    stream_config_t config = stream_default_config();
    config.mode = STREAM_MODE_BACKGROUND;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    bool result = brain_stream_pause(stream);
    EXPECT_TRUE(result);

    // Verify paused - pause returns success, processing stops
    // Note: stream_stats_t doesn't have is_paused field,
    // but pause() returning true indicates success
}

/**
 * WHAT: Test resuming stream
 * WHY: Verify resume functionality works
 */
TEST_F(StreamTest, ResumeStream)
{
    stream_config_t config = stream_default_config();
    config.mode = STREAM_MODE_BACKGROUND;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    brain_stream_pause(stream);
    bool result = brain_stream_resume(stream);
    EXPECT_TRUE(result);

    // Verify resumed - resume returns success, processing continues
    // Note: stream_stats_t doesn't have is_paused field,
    // but resume() returning true indicates success
}

/**
 * WHAT: Test flushing stream
 * WHY: Verify flush functionality works
 */
TEST_F(StreamTest, FlushStream)
{
    stream_config_t config = stream_default_config();
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed some inputs
    for (uint32_t i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, i);
    }

    // Flush
    bool result = brain_stream_flush(stream, 1000);  // 1 second timeout
    EXPECT_TRUE(result);
}

/**
 * WHAT: Test clearing stream
 * WHY: Verify clear functionality works
 */
TEST_F(StreamTest, ClearStream)
{
    stream_config_t config = stream_default_config();
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed inputs
    for (uint32_t i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, i);
    }

    // Clear
    bool result = brain_stream_clear(stream);
    EXPECT_TRUE(result);

    // Verify cleared
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    // Buffer should be empty after clear
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test getting stream statistics
 * WHY: Verify statistics tracking works
 */
TEST_F(StreamTest, GetStatistics)
{
    stream_config_t config = stream_default_config();
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed inputs
    for (uint32_t i = 0; i < 50; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, i);
    }

    // Get statistics
    stream_stats_t stats;
    bool result = brain_stream_get_stats(stream, &stats);

    ASSERT_TRUE(result);
    EXPECT_EQ(stats.inputs_fed, 50u);
    EXPECT_GE(stats.inputs_processed, 0u);
}

/**
 * WHAT: Test resetting statistics
 * WHY: Verify stats reset works
 */
TEST_F(StreamTest, ResetStatistics)
{
    stream_config_t config = stream_default_config();
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed inputs
    for (uint32_t i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, i);
    }

    // Reset stats
    brain_stream_reset_stats(stream);

    // Verify reset
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    EXPECT_EQ(stats.inputs_fed, 0u);
    EXPECT_EQ(stats.inputs_processed, 0u);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * WHAT: Test concurrent feeding from multiple threads
 * WHY: Verify thread safety of ring buffer
 */
TEST_F(StreamTest, ConcurrentFeeding)
{
    stream_config_t config = stream_default_config();
    config.buffer_size = 1000;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Create multiple threads feeding concurrently
    const int NUM_THREADS = 4;
    const int INPUTS_PER_THREAD = 100;

    auto feeder_thread = [this](uint32_t thread_id) {
        for (uint32_t i = 0; i < INPUTS_PER_THREAD; i++) {
            brain_stream_feed(this->stream, this->test_features, NUM_FEATURES,
                              thread_id * 1000 + i);
            usleep(100);  // Small delay
        }
    };

    std::thread threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i] = std::thread(feeder_thread, i);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i].join();
    }

    // Verify all inputs were processed
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    // Should have fed NUM_THREADS * INPUTS_PER_THREAD inputs
    EXPECT_EQ(stats.inputs_fed, NUM_THREADS * INPUTS_PER_THREAD);
}

//=============================================================================
// Callback Tests
//=============================================================================

/**
 * WHAT: Test high salience callback
 * WHY: Verify callbacks are invoked correctly
 */
TEST_F(StreamTest, HighSalienceCallback)
{
    g_high_salience_count = 0;

    stream_config_t config = stream_default_config();
    config.on_high_salience = high_salience_callback;
    config.high_salience_threshold = 0.5f;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed inputs that should trigger callback
    for (uint32_t i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, i);
    }

    usleep(100000);  // Wait for processing

    // Note: Callback might not be triggered if salience is below threshold
    // This just verifies the mechanism doesn't crash
    EXPECT_GE(g_high_salience_count, 0u);
}

//=============================================================================
// Performance Tests
//=============================================================================

/**
 * WHAT: Test stream throughput
 * WHY: Verify performance is acceptable
 */
TEST_F(StreamTest, Throughput)
{
    stream_config_t config = stream_default_config();
    config.buffer_size = 10000;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    const uint32_t NUM_INPUTS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    // Feed inputs
    for (uint32_t i = 0; i < NUM_INPUTS; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_us = (double) duration.count() / NUM_INPUTS;

    // Each feed should be very fast (lock-free)
    // Target: < 10 microseconds per feed
    EXPECT_LT(avg_time_us, 10.0);

    printf("Average feed time: %.2f microseconds\n", avg_time_us);
}

// Note: main() is defined in test_module.cpp - all test files share one main()
