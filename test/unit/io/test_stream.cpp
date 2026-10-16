/**
 * @file test_stream.cpp
 * @brief Comprehensive unit tests for continuous input streaming (nimcp_stream)
 *
 * Test coverage:
 * - Stream creation and configuration
 * - Stream modes (synchronous, background, batched)
 * - Input feeding (single and batch)
 * - Ring buffer operations
 * - Decision caching and retrieval
 * - Statistics tracking
 * - Stream control (pause, resume, flush, clear)
 * - Event callbacks
 * - Thread safety
 * - Edge cases and error handling
 * - Memory management
 */

#include <gtest/gtest.h>
#include <cmath>
#include <atomic>
#include <thread>
#include <chrono>
#include <unistd.h>
#include "utils/nimcp_test_base.h"
#include "io/stream/nimcp_stream.h"
#include "core/brain/nimcp_brain.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class StreamTest : public ::testing::Test {
protected:
    // Shared brain across all tests (heavyweight to create/destroy)
    static brain_t shared_brain;
    brain_t brain;  // Per-test alias (points to shared_brain)
    brain_stream_t stream;
    stream_config_t config;

    // Test data
    static constexpr uint32_t NUM_FEATURES = 13;
    float test_features[NUM_FEATURES];

    // Event callback tracking
    static std::atomic<int> high_salience_count;
    static std::atomic<int> high_surprise_count;
    static std::atomic<int> decision_ready_count;
    static std::atomic<int> buffer_full_count;
    static std::atomic<int> error_count;

    // Suite-level setup: create brain once for all tests
    static void SetUpTestSuite() {
        shared_brain = brain_create("test_stream", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, NUM_FEATURES, 5);
    }

    // Suite-level teardown: destroy brain after all tests
    static void TearDownTestSuite() {
        if (shared_brain) {
            brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    void SetUp() override {
        // Use shared brain
        brain = shared_brain;
        ASSERT_NE(brain, nullptr);

        // Initialize test features
        for (uint32_t i = 0; i < NUM_FEATURES; i++) {
            test_features[i] = (float)i / NUM_FEATURES;
        }

        // Get default stream config
        config = stream_default_config();

        // Reset callback counters
        high_salience_count = 0;
        high_surprise_count = 0;
        decision_ready_count = 0;
        buffer_full_count = 0;
        error_count = 0;

        stream = nullptr;
    }

    void TearDown() override {
        if (stream) {
            brain_destroy_stream(stream);
            stream = nullptr;
        }

        // Don't destroy brain here - it's shared across all tests
        brain = nullptr;

        // Ensure stream module is properly shut down
        stream_shutdown();
    }

    // Event callback handlers
    static void on_high_salience(const stream_event_t* event, void* context) {
        high_salience_count++;
    }

    static void on_high_surprise(const stream_event_t* event, void* context) {
        high_surprise_count++;
    }

    static void on_decision_ready(const stream_event_t* event, void* context) {
        decision_ready_count++;
    }

    static void on_buffer_full(const stream_event_t* event, void* context) {
        buffer_full_count++;
    }

    static void on_error(const stream_event_t* event, void* context) {
        error_count++;
    }
};

// Initialize static members
brain_t StreamTest::shared_brain{nullptr};
std::atomic<int> StreamTest::high_salience_count{0};
std::atomic<int> StreamTest::high_surprise_count{0};
std::atomic<int> StreamTest::decision_ready_count{0};
std::atomic<int> StreamTest::buffer_full_count{0};
std::atomic<int> StreamTest::error_count{0};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(StreamTest, DefaultConfigHasReasonableValues) {
    // WHAT: Verify default configuration is sensible
    // WHY:  Configuration baseline test

    stream_config_t cfg = stream_default_config();

    EXPECT_EQ(cfg.mode, STREAM_MODE_SYNCHRONOUS);
    EXPECT_EQ(cfg.buffer_size, 1024u);
    EXPECT_FALSE(cfg.drop_on_full);
    EXPECT_EQ(cfg.processing_interval_ms, 100u);
    EXPECT_EQ(cfg.batch_size, 10u);
    EXPECT_FLOAT_EQ(cfg.high_salience_threshold, 0.8f);
    EXPECT_FLOAT_EQ(cfg.high_surprise_threshold, 0.8f);
    EXPECT_TRUE(cfg.enable_decision_caching);
    EXPECT_TRUE(cfg.enable_salience_evaluation);
}

TEST_F(StreamTest, CustomConfigPreservesSettings) {
    // WHAT: Custom configuration is honored
    // WHY:  Config validation test

    config.mode = STREAM_MODE_BACKGROUND;
    config.buffer_size = 256;
    config.processing_interval_ms = 50;
    config.high_salience_threshold = 0.9f;

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Verify by checking behavior (buffer capacity should be power of 2)
    stream_stats_t stats;
    ASSERT_TRUE(brain_stream_get_stats(stream, &stats));
    EXPECT_EQ(stats.queue_capacity, 256u);
}

//=============================================================================
// Stream Creation and Destruction Tests
//=============================================================================

TEST_F(StreamTest, CreateStreamSynchronousMode) {
    // WHAT: Create stream in synchronous mode
    // WHY:  Basic factory pattern test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);

    ASSERT_NE(stream, nullptr);
    EXPECT_STREQ(brain_stream_get_last_error(), "");
}

TEST_F(StreamTest, CreateStreamBackgroundMode) {
    // WHAT: Create stream with background thread
    // WHY:  Thread creation test

    config.mode = STREAM_MODE_BACKGROUND;
    config.processing_interval_ms = 10;
    stream = brain_create_stream(brain, &config);

    ASSERT_NE(stream, nullptr);

    // Give thread time to start
    usleep(50000);  // 50ms
}

TEST_F(StreamTest, CreateStreamBatchedMode) {
    // WHAT: Create stream with batched processing
    // WHY:  Batch mode test

    config.mode = STREAM_MODE_BATCHED;
    config.batch_size = 5;
    stream = brain_create_stream(brain, &config);

    ASSERT_NE(stream, nullptr);
}

TEST_F(StreamTest, CreateStreamWithNullBrainFails) {
    // WHAT: NULL brain should fail
    // WHY:  Input validation test

    stream = brain_create_stream(nullptr, &config);

    EXPECT_EQ(stream, nullptr);
    EXPECT_STRNE(brain_stream_get_last_error(), "");
}

TEST_F(StreamTest, CreateStreamWithNullConfigFails) {
    // WHAT: NULL config should fail
    // WHY:  Input validation test

    stream = brain_create_stream(brain, nullptr);

    EXPECT_EQ(stream, nullptr);
    EXPECT_STRNE(brain_stream_get_last_error(), "");
}

TEST_F(StreamTest, CreateStreamWithZeroBufferSizeFails) {
    // WHAT: Zero buffer size is invalid
    // WHY:  Configuration validation

    config.buffer_size = 0;
    stream = brain_create_stream(brain, &config);

    EXPECT_EQ(stream, nullptr);
}

TEST_F(StreamTest, CreateStreamRoundsBufferSizeToPowerOfTwo) {
    // WHAT: Non-power-of-2 buffer sizes are rounded up
    // WHY:  Ring buffer requirement

    config.buffer_size = 100;  // Will be rounded to 128
    stream = brain_create_stream(brain, &config);

    ASSERT_NE(stream, nullptr);

    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    // Should be rounded to next power of 2
    EXPECT_EQ(stats.queue_capacity, 128u);
}

TEST_F(StreamTest, DestroyStreamCleansUpResources) {
    // WHAT: Destroy frees all resources
    // WHY:  Memory management test

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    brain_destroy_stream(stream);
    stream = nullptr;

    // Check memory tracking shows no leaks (if enabled)
    SUCCEED();
}

TEST_F(StreamTest, DestroyNullStreamIsSafe) {
    // WHAT: Destroying NULL stream doesn't crash
    // WHY:  Defensive programming test

    brain_destroy_stream(nullptr);

    SUCCEED();
}

//=============================================================================
// Stream Input Tests (Single Feed)
//=============================================================================

TEST_F(StreamTest, FeedInputSynchronousMode) {
    // WHAT: Feed input in synchronous mode processes immediately
    // WHY:  Synchronous processing test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    uint64_t timestamp = nimcp_time_monotonic_ns();
    bool result = brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);

    EXPECT_TRUE(result);

    // Check stats
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    EXPECT_EQ(stats.inputs_fed, 1u);
    EXPECT_EQ(stats.inputs_processed, 1u);
}

TEST_F(StreamTest, FeedInputBackgroundMode) {
    // WHAT: Feed input in background mode enqueues
    // WHY:  Asynchronous processing test

    config.mode = STREAM_MODE_BACKGROUND;
    config.processing_interval_ms = 10;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    uint64_t timestamp = nimcp_time_monotonic_ns();
    bool result = brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);

    EXPECT_TRUE(result);

    // Wait for processing
    usleep(100000);  // 100ms

    // Check stats
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    EXPECT_EQ(stats.inputs_fed, 1u);
    EXPECT_GE(stats.inputs_processed, 1u);
}

TEST_F(StreamTest, FeedInputWithNullStreamFails) {
    // WHAT: NULL stream returns false
    // WHY:  Input validation

    bool result = brain_stream_feed(nullptr, test_features, NUM_FEATURES, 0);

    EXPECT_FALSE(result);
}

TEST_F(StreamTest, FeedInputWithNullFeaturesFails) {
    // WHAT: NULL features returns false
    // WHY:  Input validation

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    bool result = brain_stream_feed(stream, nullptr, NUM_FEATURES, 0);

    EXPECT_FALSE(result);
}

TEST_F(StreamTest, FeedMultipleInputs) {
    // WHAT: Multiple inputs are tracked correctly
    // WHY:  Statistics accumulation test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp + i * 1000000);
    }

    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    EXPECT_EQ(stats.inputs_fed, 10u);
    EXPECT_EQ(stats.inputs_processed, 10u);
}

//=============================================================================
// Batch Input Tests
//=============================================================================

TEST_F(StreamTest, FeedBatchOfInputs) {
    // WHAT: Batch feed enqueues multiple inputs
    // WHY:  Batch processing test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    const uint32_t num_samples = 5;
    float* features[num_samples];
    uint64_t timestamps[num_samples];

    for (uint32_t i = 0; i < num_samples; i++) {
        features[i] = test_features;
        timestamps[i] = nimcp_time_monotonic_ns() + i * 1000000;
    }

    uint32_t enqueued = brain_stream_feed_batch(stream, (const float**)features,
                                                num_samples, NUM_FEATURES, timestamps);

    EXPECT_EQ(enqueued, num_samples);

    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    EXPECT_EQ(stats.inputs_fed, num_samples);
}

TEST_F(StreamTest, FeedBatchWithNullStreamReturnsZero) {
    // WHAT: NULL stream returns 0
    // WHY:  Input validation

    float* features[1] = {test_features};
    uint64_t timestamps[1] = {0};

    uint32_t enqueued = brain_stream_feed_batch(nullptr, (const float**)features,
                                                1, NUM_FEATURES, timestamps);

    EXPECT_EQ(enqueued, 0u);
}

//=============================================================================
// Buffer Management Tests
//=============================================================================

TEST_F(StreamTest, BufferFullBlocksByDefault) {
    // WHAT: When buffer is full, feed blocks/fails
    // WHY:  Backpressure handling test

    config.mode = STREAM_MODE_BACKGROUND;
    config.buffer_size = 16;  // Small buffer
    config.drop_on_full = false;
    config.processing_interval_ms = 1000;  // Slow processing
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Fill buffer
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 20; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    // Some inputs should be dropped
    EXPECT_GT(stats.inputs_dropped, 0u);
}

TEST_F(StreamTest, BufferFullDropsOldestWhenConfigured) {
    // WHAT: drop_on_full causes old entries to be dropped
    // WHY:  Backpressure strategy test

    config.mode = STREAM_MODE_BACKGROUND;
    config.buffer_size = 16;
    config.drop_on_full = true;
    config.processing_interval_ms = 1000;  // Slow processing
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Overfill buffer
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 30; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    // Old entries should be dropped
    EXPECT_GT(stats.inputs_dropped, 0u);
}

TEST_F(StreamTest, QueueSizeReflectsBufferState) {
    // WHAT: Queue size matches pending inputs
    // WHY:  Queue monitoring test

    config.mode = STREAM_MODE_BACKGROUND;
    config.processing_interval_ms = 1000;  // Slow processing
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Add inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 5; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    EXPECT_GT(stats.queue_size, 0u);
    EXPECT_LE(stats.queue_size, 5u);
}

//=============================================================================
// Decision and Salience Tests
//=============================================================================

TEST_F(StreamTest, GetDecisionReturnsNullAfterDeprecation) {
    // WHAT: brain_stream_get_decision now returns NULL (deprecated)
    // WHY:  API deprecation test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    brain_stream_feed(stream, test_features, NUM_FEATURES, nimcp_time_monotonic_ns());

    brain_decision_t* decision = brain_stream_get_decision(stream);

    // Function is deprecated and returns NULL
    EXPECT_EQ(decision, nullptr);
}

TEST_F(StreamTest, GetSalienceReturnsValidValue) {
    // WHAT: Salience is updated after processing
    // WHY:  Salience tracking test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.enable_salience_evaluation = true;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    brain_stream_feed(stream, test_features, NUM_FEATURES, nimcp_time_monotonic_ns());

    float salience = brain_stream_get_salience(stream);

    EXPECT_GE(salience, 0.0f);
    EXPECT_LE(salience, 1.0f);
}

TEST_F(StreamTest, GetSalienceWithNullStreamReturnsNegative) {
    // WHAT: NULL stream returns -1.0
    // WHY:  Error handling test

    float salience = brain_stream_get_salience(nullptr);

    EXPECT_FLOAT_EQ(salience, -1.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(StreamTest, GetStatsReturnsValidData) {
    // WHAT: Statistics reflect stream activity
    // WHY:  Monitoring test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Process some inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 5; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    stream_stats_t stats;
    bool result = brain_stream_get_stats(stream, &stats);

    EXPECT_TRUE(result);
    EXPECT_EQ(stats.inputs_fed, 5u);
    EXPECT_EQ(stats.inputs_processed, 5u);
    EXPECT_EQ(stats.decisions_generated, 5u);
    EXPECT_GE(stats.avg_processing_time_ms, 0.0f);
}

TEST_F(StreamTest, ResetStatsClearsCounters) {
    // WHAT: Reset stats zeros all counters
    // WHY:  Statistics reset test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Build up stats
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // Reset
    bool result = brain_stream_reset_stats(stream);
    EXPECT_TRUE(result);

    // Check stats are cleared
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    EXPECT_EQ(stats.inputs_fed, 0u);
    EXPECT_EQ(stats.inputs_processed, 0u);
    EXPECT_EQ(stats.inputs_dropped, 0u);
    EXPECT_EQ(stats.decisions_generated, 0u);
}

TEST_F(StreamTest, GetStatsWithNullStreamFails) {
    // WHAT: NULL stream returns false
    // WHY:  Input validation

    stream_stats_t stats;
    bool result = brain_stream_get_stats(nullptr, &stats);

    EXPECT_FALSE(result);
}

TEST_F(StreamTest, GetStatsWithNullStatsParamFails) {
    // WHAT: NULL stats param returns false
    // WHY:  Input validation

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    bool result = brain_stream_get_stats(stream, nullptr);

    EXPECT_FALSE(result);
}

//=============================================================================
// Stream Control Tests
//=============================================================================

TEST_F(StreamTest, PauseStopsProcessing) {
    // WHAT: Pause halts background processing
    // WHY:  Control flow test

    config.mode = STREAM_MODE_BACKGROUND;
    config.processing_interval_ms = 10;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Pause immediately
    bool result = brain_stream_pause(stream);
    EXPECT_TRUE(result);

    // Feed inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 5; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // Wait a bit
    usleep(100000);  // 100ms

    // Inputs should be queued but not processed
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    EXPECT_EQ(stats.inputs_fed, 5u);
    EXPECT_LT(stats.inputs_processed, stats.inputs_fed);  // Less processed
}

TEST_F(StreamTest, ResumeRestartsProcessing) {
    // WHAT: Resume continues processing after pause
    // WHY:  Control flow test

    config.mode = STREAM_MODE_BACKGROUND;
    config.processing_interval_ms = 10;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    brain_stream_pause(stream);

    // Feed inputs while paused
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 5; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // Resume
    bool result = brain_stream_resume(stream);
    EXPECT_TRUE(result);

    // Wait for processing
    usleep(200000);  // 200ms

    // Now inputs should be processed
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    EXPECT_GE(stats.inputs_processed, 1u);
}

TEST_F(StreamTest, FlushWaitsForQueueToDrain) {
    // WHAT: Flush blocks until queue is empty
    // WHY:  Synchronization test

    config.mode = STREAM_MODE_BACKGROUND;
    config.processing_interval_ms = 10;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // Flush with timeout
    bool result = brain_stream_flush(stream, 1000);  // 1 second timeout

    EXPECT_TRUE(result);

    // Queue should be empty
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    EXPECT_EQ(stats.queue_size, 0u);
}

TEST_F(StreamTest, FlushTimeoutWhenQueueDoesNotDrain) {
    // WHAT: Flush times out if queue doesn't drain
    // WHY:  Timeout handling test

    config.mode = STREAM_MODE_BACKGROUND;
    config.processing_interval_ms = 10000;  // Very slow
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Pause to prevent processing
    brain_stream_pause(stream);

    // Fill queue
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // Try to flush with short timeout
    bool result = brain_stream_flush(stream, 50);  // 50ms timeout

    EXPECT_FALSE(result);  // Should timeout
}

TEST_F(StreamTest, ClearDropsAllPendingInputs) {
    // WHAT: Clear empties the queue
    // WHY:  Queue reset test

    config.mode = STREAM_MODE_BACKGROUND;
    config.processing_interval_ms = 10000;  // Very slow
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Pause to prevent processing
    brain_stream_pause(stream);

    // Fill queue
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // Clear
    bool result = brain_stream_clear(stream);
    EXPECT_TRUE(result);

    // Queue should be empty
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    EXPECT_EQ(stats.queue_size, 0u);
}

TEST_F(StreamTest, ControlFunctionsWithNullStreamReturnFalse) {
    // WHAT: NULL stream returns false for all control functions
    // WHY:  Input validation

    EXPECT_FALSE(brain_stream_pause(nullptr));
    EXPECT_FALSE(brain_stream_resume(nullptr));
    EXPECT_FALSE(brain_stream_flush(nullptr, 100));
    EXPECT_FALSE(brain_stream_clear(nullptr));
}

//=============================================================================
// Event Callback Tests
//=============================================================================

TEST_F(StreamTest, DecisionReadyCallbackIsCalled) {
    // WHAT: Callback is invoked when decision is ready
    // WHY:  Observer pattern test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.on_decision_ready = on_decision_ready;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    decision_ready_count = 0;

    // Feed input
    brain_stream_feed(stream, test_features, NUM_FEATURES, nimcp_time_monotonic_ns());

    // Callback should be called
    EXPECT_EQ(decision_ready_count.load(), 1);
}

TEST_F(StreamTest, HighSalienceCallbackIsCalledWhenThresholdExceeded) {
    // WHAT: High salience callback fires above threshold
    // WHY:  Threshold event test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.enable_salience_evaluation = true;
    config.high_salience_threshold = 0.0f;  // Low threshold to trigger
    config.on_high_salience = on_high_salience;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    high_salience_count = 0;

    // Feed multiple inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 5; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // At least some should trigger callback
    EXPECT_GE(high_salience_count.load(), 1);
}

TEST_F(StreamTest, CallbacksReceiveCorrectEventData) {
    // WHAT: Event data passed to callbacks is valid
    // WHY:  Event data integrity test

    static const stream_event_t* last_event = nullptr;

    auto custom_callback = [](const stream_event_t* event, void* context) {
        last_event = event;
        EXPECT_NE(event, nullptr);
        EXPECT_EQ(event->type, STREAM_EVENT_DECISION_READY);
        EXPECT_GT(event->timestamp, 0u);
        EXPECT_NE(event->message, nullptr);
    };

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.on_decision_ready = custom_callback;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    brain_stream_feed(stream, test_features, NUM_FEATURES, nimcp_time_monotonic_ns());

    // Custom callback verified event data
}

TEST_F(StreamTest, CallbackContextIsPassedThrough) {
    // WHAT: Callback context pointer is passed correctly
    // WHY:  Context handling test

    static void* received_context = nullptr;
    int my_context = 42;

    auto callback_with_context = [](const stream_event_t* event, void* context) {
        received_context = context;
    };

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.on_decision_ready = callback_with_context;
    config.callback_context = &my_context;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    brain_stream_feed(stream, test_features, NUM_FEATURES, nimcp_time_monotonic_ns());

    EXPECT_EQ(received_context, &my_context);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(StreamTest, ConcurrentFeedFromMultipleThreads) {
    // WHAT: Multiple threads can feed inputs concurrently
    // WHY:  Thread safety test

    config.mode = STREAM_MODE_BACKGROUND;
    config.buffer_size = 1024;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    const int num_threads = 4;
    const int feeds_per_thread = 25;
    std::thread threads[num_threads];

    for (int t = 0; t < num_threads; t++) {
        threads[t] = std::thread([this, feeds_per_thread]() {
            for (int i = 0; i < feeds_per_thread; i++) {
                brain_stream_feed(stream, test_features, NUM_FEATURES,
                                nimcp_time_monotonic_ns());
                usleep(1000);  // 1ms delay
            }
        });
    }

    // Wait for all threads
    for (int t = 0; t < num_threads; t++) {
        threads[t].join();
    }

    // Wait for processing
    brain_stream_flush(stream, 2000);

    // Verify all inputs were fed
    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);
    EXPECT_EQ(stats.inputs_fed, (uint64_t)(num_threads * feeds_per_thread));
}

TEST_F(StreamTest, ConcurrentStatsAccess) {
    // WHAT: Statistics can be read while processing
    // WHY:  Concurrent access test

    config.mode = STREAM_MODE_BACKGROUND;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    std::atomic<bool> stop{false};

    // Producer thread
    std::thread producer([this, &stop]() {
        while (!stop) {
            brain_stream_feed(stream, test_features, NUM_FEATURES,
                            nimcp_time_monotonic_ns());
            usleep(1000);
        }
    });

    // Stats reader thread
    std::thread reader([this, &stop]() {
        stream_stats_t stats;
        while (!stop) {
            brain_stream_get_stats(stream, &stats);
            usleep(5000);
        }
    });

    // Run for a bit
    usleep(100000);  // 100ms
    stop = true;

    producer.join();
    reader.join();

    SUCCEED();
}

//=============================================================================
// Batched Mode Tests
//=============================================================================

TEST_F(StreamTest, BatchedModeProcessesInBatches) {
    // WHAT: Batched mode accumulates inputs before processing
    // WHY:  Batch processing strategy test

    config.mode = STREAM_MODE_BATCHED;
    config.batch_size = 5;
    config.processing_interval_ms = 10;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed less than batch size
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 3; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // Wait a bit
    usleep(50000);  // 50ms

    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    // Some or all may be processed depending on timing
    EXPECT_EQ(stats.inputs_fed, 3u);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(StreamTest, HandlesEmptyFeatureVector) {
    // WHAT: Zero-length features should fail gracefully
    // WHY:  Edge case test

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    bool result = brain_stream_feed(stream, test_features, 0, nimcp_time_monotonic_ns());

    // Behavior depends on implementation - should not crash
    SUCCEED();
}

TEST_F(StreamTest, HandlesVeryLargeTimestamp) {
    // WHAT: Large timestamp values don't cause issues
    // WHY:  Boundary value test

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    uint64_t large_timestamp = UINT64_MAX - 1000;
    bool result = brain_stream_feed(stream, test_features, NUM_FEATURES, large_timestamp);

    EXPECT_TRUE(result);
}

TEST_F(StreamTest, HandlesRapidCreateDestroy) {
    // WHAT: Rapid create/destroy cycles don't leak
    // WHY:  Resource management stress test

    for (int i = 0; i < 10; i++) {
        stream = brain_create_stream(brain, &config);
        ASSERT_NE(stream, nullptr);

        brain_stream_feed(stream, test_features, NUM_FEATURES, nimcp_time_monotonic_ns());

        brain_destroy_stream(stream);
        stream = nullptr;
    }

    SUCCEED();
}

TEST_F(StreamTest, DestroyWhileProcessing) {
    // WHAT: Destroying stream while processing is safe
    // WHY:  Shutdown race condition test

    config.mode = STREAM_MODE_BACKGROUND;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed many inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 100; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // Immediately destroy
    brain_destroy_stream(stream);
    stream = nullptr;

    SUCCEED();
}

TEST_F(StreamTest, ErrorMessageIsThreadLocal) {
    // WHAT: Error messages don't interfere between threads
    // WHY:  Thread-local storage test

    std::atomic<int> errors{0};

    auto thread_func = [&errors]() {
        // Trigger an error
        brain_stream_t bad_stream = brain_create_stream(nullptr, nullptr);

        const char* error = brain_stream_get_last_error();
        if (error && strlen(error) > 0) {
            errors++;
        }
    };

    std::thread t1(thread_func);
    std::thread t2(thread_func);

    t1.join();
    t2.join();

    EXPECT_EQ(errors.load(), 2);  // Both threads saw errors
}

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(StreamTest, StreamDoesNotLeakMemoryOnNormalUsage) {
    // WHAT: Normal usage doesn't leak memory
    // WHY:  Memory leak test

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Process many inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 100; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    brain_destroy_stream(stream);
    stream = nullptr;

    // Memory tracking (if enabled) should show no leaks
    SUCCEED();
}

TEST_F(StreamTest, PendingInputsAreFreedOnDestroy) {
    // WHAT: Unprocessed inputs are freed on destroy
    // WHY:  Cleanup test

    config.mode = STREAM_MODE_BACKGROUND;
    config.processing_interval_ms = 10000;  // Very slow
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Pause to prevent processing
    brain_stream_pause(stream);

    // Fill queue
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 50; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    // Destroy with pending inputs
    brain_destroy_stream(stream);
    stream = nullptr;

    SUCCEED();
}

//=============================================================================
// Performance/Throughput Tests
//=============================================================================

TEST_F(StreamTest, ThroughputMeasurement) {
    // WHAT: Measure inputs processed per second
    // WHY:  Performance baseline

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    const int num_inputs = 100;
    uint64_t start_time = nimcp_time_monotonic_ns();

    uint64_t timestamp = start_time;
    for (int i = 0; i < num_inputs; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    uint64_t elapsed_ns = nimcp_time_elapsed_ns(start_time);
    double elapsed_sec = elapsed_ns / 1e9;
    double throughput = num_inputs / elapsed_sec;

    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    // Should have reasonable throughput
    EXPECT_GT(throughput, 0.0);
    EXPECT_EQ(stats.inputs_processed, (uint64_t)num_inputs);
}

TEST_F(StreamTest, AverageProcessingTimeIsTracked) {
    // WHAT: Average processing time is calculated
    // WHY:  Performance monitoring

    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 10; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);
    }

    stream_stats_t stats;
    brain_stream_get_stats(stream, &stats);

    EXPECT_GT(stats.avg_processing_time_ms, 0.0f);
    EXPECT_LT(stats.avg_processing_time_ms, 1000.0f);  // Should be under 1 second
}

//=============================================================================
// Phase IO-1: Memory Integration Tests
//=============================================================================

TEST_F(StreamTest, StreamWithUnifiedMemory) {
    // WHAT: Stream with unified memory enabled
    // WHY:  Verify memory integration works correctly

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.use_unified_memory = true;  // Enable memory integration

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed some inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 5; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp + i);
    }

    // Get extended statistics
    stream_extended_stats_t ext_stats;
    bool result = brain_stream_get_extended_stats(stream, &ext_stats);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ext_stats.using_unified_memory);
}

TEST_F(StreamTest, StreamExtendedStatsBasic) {
    // WHAT: Extended statistics without integration
    // WHY:  Verify stats work without memory/security

    config.mode = STREAM_MODE_SYNCHRONOUS;

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed some inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);

    // Get extended statistics
    stream_extended_stats_t ext_stats;
    bool result = brain_stream_get_extended_stats(stream, &ext_stats);
    EXPECT_TRUE(result);

    // Base stats should be populated
    EXPECT_GE(ext_stats.base.inputs_processed, 0u);

    // Memory integration should be disabled
    EXPECT_FALSE(ext_stats.using_unified_memory);

    // Security integration should be disabled
    EXPECT_FALSE(ext_stats.security_registered);
}

TEST_F(StreamTest, StreamExtendedStatsNullParams) {
    // WHAT: Extended stats with NULL parameters
    // WHY:  Verify error handling

    stream_extended_stats_t ext_stats;

    // NULL stream
    EXPECT_FALSE(brain_stream_get_extended_stats(nullptr, &ext_stats));

    // NULL stats
    config.mode = STREAM_MODE_SYNCHRONOUS;
    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);
    EXPECT_FALSE(brain_stream_get_extended_stats(stream, nullptr));
}

TEST_F(StreamTest, ExternalMemoryManager) {
    // WHAT: Stream with external memory manager
    // WHY:  Verify user-provided memory manager works

    // Create external memory manager
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = true;
    unified_mem_manager_t ext_manager = unified_mem_create(&mem_config);
    ASSERT_NE(ext_manager, nullptr);

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.use_unified_memory = true;
    config.memory_manager = ext_manager;

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed a few inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp);

    // Verify external manager is used
    stream_extended_stats_t ext_stats;
    EXPECT_TRUE(brain_stream_get_extended_stats(stream, &ext_stats));
    EXPECT_TRUE(ext_stats.using_unified_memory);

    // Clean up - destroy stream first
    brain_destroy_stream(stream);
    stream = nullptr;

    // External manager should still be valid
    unified_mem_destroy(ext_manager);
}

//=============================================================================
// Phase IO-2: Security Integration Tests
//=============================================================================

TEST_F(StreamTest, ModuleInitShutdown) {
    // WHAT: Module initialization and shutdown
    // WHY:  Verify module lifecycle management

    // Initialize without security context
    nimcp_result_t result = stream_init(nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Get module ID (should be 0 without security)
    uint32_t module_id = stream_get_security_module_id();
    EXPECT_EQ(module_id, 0u);

    // Shutdown
    stream_shutdown();

    // Re-initialization should work
    result = stream_init(nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    stream_shutdown();
}

TEST_F(StreamTest, DoubleModuleInit) {
    // WHAT: Double initialization
    // WHY:  Verify idempotent initialization

    nimcp_result_t result = stream_init(nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Second init should succeed (no-op)
    result = stream_init(nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    stream_shutdown();
}

TEST_F(StreamTest, ShutdownWithoutInit) {
    // WHAT: Shutdown without init
    // WHY:  Verify safe shutdown when not initialized

    // Should not crash
    EXPECT_NO_THROW({ stream_shutdown(); });
}

TEST_F(StreamTest, StreamSecurityNoContext) {
    // WHAT: Stream with security enabled but no context
    // WHY:  Verify graceful handling when security not available

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.enable_security = true;  // Enable but no context

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Should work but security not registered
    stream_extended_stats_t ext_stats;
    EXPECT_TRUE(brain_stream_get_extended_stats(stream, &ext_stats));
    EXPECT_FALSE(ext_stats.security_registered);
}

//=============================================================================
// Combined Memory and Security Tests
//=============================================================================

TEST_F(StreamTest, FullIntegrationStack) {
    // WHAT: Stream with both memory and security enabled
    // WHY:  Verify full integration stack works together

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.use_unified_memory = true;
    config.enable_security = true;

    stream = brain_create_stream(brain, &config);
    ASSERT_NE(stream, nullptr);

    // Feed many inputs
    uint64_t timestamp = nimcp_time_monotonic_ns();
    for (int i = 0; i < 50; i++) {
        brain_stream_feed(stream, test_features, NUM_FEATURES, timestamp + i * 1000);
    }

    // Verify stats
    stream_extended_stats_t ext_stats;
    EXPECT_TRUE(brain_stream_get_extended_stats(stream, &ext_stats));
    EXPECT_TRUE(ext_stats.using_unified_memory);
    EXPECT_GE(ext_stats.base.inputs_processed, 50u);
}

TEST_F(StreamTest, StreamMemoryCleanupOnDestroy) {
    // WHAT: Memory is properly cleaned up on destroy
    // WHY:  Verify no memory leaks with integration

    config.mode = STREAM_MODE_SYNCHRONOUS;
    config.use_unified_memory = true;

    // Create and destroy multiple times
    for (int i = 0; i < 10; i++) {
        brain_stream_t s = brain_create_stream(brain, &config);
        ASSERT_NE(s, nullptr);

        // Feed some data
        uint64_t timestamp = nimcp_time_monotonic_ns();
        brain_stream_feed(s, test_features, NUM_FEATURES, timestamp);

        brain_destroy_stream(s);
    }

    // If no crashes or memory issues, test passes
    SUCCEED();
}
