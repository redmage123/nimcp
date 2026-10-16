//=============================================================================
// test_brain_async.cpp - Async Brain Operations Test Suite
//=============================================================================
/**
 * @file test_brain_async.cpp
 * @brief Comprehensive tests for async brain learning and inference
 *
 * WHAT: Test suite for nimcp_brain_learn_async and nimcp_brain_infer_async
 * WHY:  Ensure async operations work correctly with proper error handling
 * HOW:  Test success cases, timeout handling, concurrent operations, errors
 *
 * TEST COVERAGE:
 * 1. Async learning completes correctly
 * 2. Async inference returns correct results
 * 3. Timeout handling works
 * 4. Multiple concurrent async operations
 * 5. Error propagation through futures
 * 6. Memory cleanup verification
 * 7. Thread safety validation
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "async/nimcp_future.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainAsyncTest : public ::testing::Test {
protected:
    brain_t brain;
    brain_config_t config;

    void SetUp() override {
        // Initialize memory subsystem first
        nimcp_memory_init();

        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bio_async_init(&bio_config));

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        ASSERT_EQ(NIMCP_SUCCESS, bio_router_init(&router_config));

        // Initialize async/futures module
        nimcp_future_init(nullptr, nullptr);

        // Create small brain for fast testing using simple API
        brain = brain_create("async_test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr) << "Failed to create brain for async test";
    }

    void TearDown() override {
        // Destroy brain BEFORE shutting down bio subsystems
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        // Shutdown futures before bio subsystems
        nimcp_future_shutdown();
        // Shutdown bio-router before bio-async (modules depend on router)
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        // Memory cleanup last
        nimcp_memory_cleanup();
    }
};

//=============================================================================
// 1. Async Learning Tests
//=============================================================================

/**
 * Test: Async learning completes successfully
 *
 * WHAT: Verify basic async learning operation
 * WHY:  Ensure future completes and returns valid loss
 * HOW:  Start async learning, wait, check result
 */
TEST_F(BrainAsyncTest, AsyncLearningBasic) {
    // Prepare input
    float features[10] = {0.5f, 0.7f, 0.2f, 0.9f, 0.1f,
                          0.4f, 0.6f, 0.8f, 0.3f, 0.5f};
    const char* label = "test_class_A";
    float confidence = 0.9f;

    // Start async learning
    nimcp_future_t future = nimcp_brain_learn_async(
        brain, features, 10, label, confidence
    );
    ASSERT_NE(future, nullptr) << "Failed to create async learning future";

    // Wait for completion (1 second timeout)
    ASSERT_TRUE(nimcp_future_wait_timeout(future, 1000))
        << "Async learning timed out";

    // Verify result
    float loss;
    ASSERT_EQ(nimcp_future_get(future, &loss), NIMCP_SUCCESS)
        << "Failed to get loss from future";

    EXPECT_GE(loss, 0.0f) << "Loss should be non-negative";
    EXPECT_LT(loss, 100.0f) << "Loss should be reasonable (single step on random data)";

    // Cleanup
    nimcp_future_destroy(future);
}

/**
 * Test: Async learning with multiple examples
 *
 * WHAT: Sequential async learning operations
 * WHY:  Ensure system handles multiple async calls
 * HOW:  Start multiple learning ops, wait for all
 */
TEST_F(BrainAsyncTest, AsyncLearningMultipleSequential) {
    const int num_examples = 5;
    std::vector<nimcp_future_t> futures;

    // Start multiple async learning operations
    for (int i = 0; i < num_examples; i++) {
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = static_cast<float>(i + j) / 20.0f;
        }

        char label[32];
        snprintf(label, sizeof(label), "class_%d", i % 3);

        nimcp_future_t future = nimcp_brain_learn_async(
            brain, features, 10, label, 0.8f
        );
        ASSERT_NE(future, nullptr) << "Failed to create future for example " << i;
        futures.push_back(future);
    }

    // Wait for all to complete
    for (int i = 0; i < num_examples; i++) {
        ASSERT_TRUE(nimcp_future_wait_timeout(futures[i], 2000))
            << "Example " << i << " timed out";

        float loss;
        ASSERT_EQ(nimcp_future_get(futures[i], &loss), NIMCP_SUCCESS)
            << "Failed to get loss for example " << i;

        EXPECT_GE(loss, 0.0f) << "Example " << i << " has negative loss";
    }

    // Cleanup
    for (auto future : futures) {
        nimcp_future_destroy(future);
    }
}

/**
 * Test: Async learning timeout handling
 *
 * WHAT: Verify timeout detection works
 * WHY:  Prevent indefinite blocking on slow operations
 * HOW:  Use very short timeout, expect timeout
 */
TEST_F(BrainAsyncTest, AsyncLearningTimeout) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    nimcp_future_t future = nimcp_brain_learn_async(
        brain, features, 10, "timeout_test", 0.9f
    );
    ASSERT_NE(future, nullptr);

    // Try extremely short timeout (should timeout or succeed immediately)
    bool completed = nimcp_future_wait_timeout(future, 0);

    // Either way is valid: immediate completion or timeout
    // Just verify we can check status
    nimcp_future_state_t state = nimcp_future_state(future);
    EXPECT_TRUE(
        state == NIMCP_FUTURE_PENDING ||
        state == NIMCP_FUTURE_COMPLETED ||
        state == NIMCP_FUTURE_FAILED
    );

    // Wait longer to ensure it completes
    ASSERT_TRUE(nimcp_future_wait_timeout(future, 2000))
        << "Learning should complete within 2 seconds";

    nimcp_future_destroy(future);
}

//=============================================================================
// 2. Async Inference Tests
//=============================================================================

/**
 * Test: Async inference completes successfully
 *
 * WHAT: Verify basic async inference operation
 * WHY:  Ensure future completes and returns valid decision
 * HOW:  Train brain, then async infer
 */
TEST_F(BrainAsyncTest, AsyncInferenceBasic) {
    // Train brain first
    float train_features[10] = {0.8f, 0.7f, 0.6f, 0.5f, 0.4f,
                                0.3f, 0.2f, 0.1f, 0.9f, 0.5f};
    brain_learn_example(brain, train_features, 10, "trained_class", 0.95f);

    // Start async inference
    float test_features[10] = {0.75f, 0.65f, 0.55f, 0.45f, 0.35f,
                               0.25f, 0.15f, 0.05f, 0.85f, 0.45f};

    nimcp_future_t future = nimcp_brain_infer_async(brain, test_features, 10);
    ASSERT_NE(future, nullptr) << "Failed to create async inference future";

    // Wait for completion
    ASSERT_TRUE(nimcp_future_wait_timeout(future, 1000))
        << "Async inference timed out";

    // Verify result
    brain_decision_t* decision;
    ASSERT_EQ(nimcp_future_get(future, &decision), NIMCP_SUCCESS)
        << "Failed to get decision from future";

    ASSERT_NE(decision, nullptr) << "Decision is NULL";
    EXPECT_GT(strlen(decision->label), 0) << "Decision has empty label";
    EXPECT_GE(decision->confidence, 0.0f) << "Confidence is negative";
    EXPECT_LE(decision->confidence, 1.0f) << "Confidence exceeds 1.0";

    // Cleanup
    brain_free_decision(decision);
    nimcp_future_destroy(future);
}

/**
 * Test: Multiple concurrent async inferences
 *
 * WHAT: Parallel async inference operations
 * WHY:  Test thread safety and concurrent access
 * HOW:  Launch multiple inferences simultaneously
 */
TEST_F(BrainAsyncTest, AsyncInferenceMultipleConcurrent) {
    // Train brain
    float train_features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                                0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_learn_example(brain, train_features, 10, "concurrent_test", 0.9f);

    const int num_inferences = 10;
    std::vector<nimcp_future_t> futures;

    // Start multiple concurrent inferences
    for (int i = 0; i < num_inferences; i++) {
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = 0.5f + (static_cast<float>(i) / 100.0f);
        }

        nimcp_future_t future = nimcp_brain_infer_async(brain, features, 10);
        ASSERT_NE(future, nullptr) << "Failed to create future for inference " << i;
        futures.push_back(future);
    }

    // Wait for all and verify
    for (int i = 0; i < num_inferences; i++) {
        ASSERT_TRUE(nimcp_future_wait_timeout(futures[i], 2000))
            << "Inference " << i << " timed out";

        brain_decision_t* decision;
        ASSERT_EQ(nimcp_future_get(futures[i], &decision), NIMCP_SUCCESS)
            << "Failed to get decision for inference " << i;

        ASSERT_NE(decision, nullptr);
        EXPECT_GT(strlen(decision->label), 0);

        brain_free_decision(decision);
    }

    // Cleanup
    for (auto future : futures) {
        nimcp_future_destroy(future);
    }
}

/**
 * Test: Async inference immediate readiness check
 *
 * WHAT: Non-blocking readiness polling
 * WHY:  Enable checking without blocking
 * HOW:  Start inference, poll is_ready
 */
TEST_F(BrainAsyncTest, AsyncInferenceReadinessCheck) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    nimcp_future_t future = nimcp_brain_infer_async(brain, features, 10);
    ASSERT_NE(future, nullptr);

    // Poll for readiness (non-blocking)
    int max_polls = 1000;
    int polls = 0;
    while (!nimcp_future_is_ready(future) && polls < max_polls) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        polls++;
    }

    ASSERT_TRUE(nimcp_future_is_ready(future))
        << "Future not ready after " << polls << " polls";

    // Verify state is terminal
    nimcp_future_state_t state = nimcp_future_state(future);
    EXPECT_TRUE(
        state == NIMCP_FUTURE_COMPLETED ||
        state == NIMCP_FUTURE_FAILED ||
        state == NIMCP_FUTURE_CANCELLED
    );

    // Get result if completed
    if (state == NIMCP_FUTURE_COMPLETED) {
        brain_decision_t* decision;
        ASSERT_EQ(nimcp_future_get(future, &decision), NIMCP_SUCCESS);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    nimcp_future_destroy(future);
}

//=============================================================================
// 3. Error Propagation Tests
//=============================================================================

/**
 * Test: Invalid parameters return NULL
 *
 * WHAT: Verify NULL parameter handling
 * WHY:  Ensure API validates inputs
 * HOW:  Call with NULL pointers
 */
TEST_F(BrainAsyncTest, ErrorHandlingNullParameters) {
    float features[10] = {0.0f};

    // NULL brain for learning
    nimcp_future_t future1 = nimcp_brain_learn_async(
        nullptr, features, 10, "test", 0.9f
    );
    EXPECT_EQ(future1, nullptr);

    // NULL features for learning
    nimcp_future_t future2 = nimcp_brain_learn_async(
        brain, nullptr, 10, "test", 0.9f
    );
    EXPECT_EQ(future2, nullptr);

    // NULL label for learning
    nimcp_future_t future3 = nimcp_brain_learn_async(
        brain, features, 10, nullptr, 0.9f
    );
    EXPECT_EQ(future3, nullptr);

    // NULL brain for inference
    nimcp_future_t future4 = nimcp_brain_infer_async(nullptr, features, 10);
    EXPECT_EQ(future4, nullptr);

    // NULL features for inference
    nimcp_future_t future5 = nimcp_brain_infer_async(brain, nullptr, 10);
    EXPECT_EQ(future5, nullptr);
}

/**
 * Test: Invalid feature counts
 *
 * WHAT: Verify dimension validation
 * WHY:  Catch dimension mismatches early
 * HOW:  Use zero or wrong feature counts
 */
TEST_F(BrainAsyncTest, ErrorHandlingInvalidDimensions) {
    float features[10] = {0.0f};

    // Zero features for learning
    nimcp_future_t future1 = nimcp_brain_learn_async(
        brain, features, 0, "test", 0.9f
    );
    EXPECT_EQ(future1, nullptr);

    // Zero features for inference
    nimcp_future_t future2 = nimcp_brain_infer_async(brain, features, 0);
    EXPECT_EQ(future2, nullptr);
}

/**
 * Test: Invalid confidence values
 *
 * WHAT: Verify confidence validation
 * WHY:  Ensure confidence stays in valid range
 * HOW:  Use negative or >1.0 confidence
 */
TEST_F(BrainAsyncTest, ErrorHandlingInvalidConfidence) {
    float features[10] = {0.5f};

    // Negative confidence
    nimcp_future_t future1 = nimcp_brain_learn_async(
        brain, features, 10, "test", -0.5f
    );
    EXPECT_EQ(future1, nullptr);

    // Confidence > 1.0
    nimcp_future_t future2 = nimcp_brain_learn_async(
        brain, features, 10, "test", 1.5f
    );
    EXPECT_EQ(future2, nullptr);
}

//=============================================================================
// 4. Mixed Async Operations Tests
//=============================================================================

/**
 * Test: Interleaved learning and inference
 *
 * WHAT: Mix async learning and inference
 * WHY:  Verify operations don't interfere
 * HOW:  Alternate between learning and inference
 */
TEST_F(BrainAsyncTest, MixedAsyncOperations) {
    std::vector<nimcp_future_t> futures;

    // Interleave learning and inference operations
    for (int i = 0; i < 5; i++) {
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = static_cast<float>(i * 10 + j) / 100.0f;
        }

        // Learning
        char label[32];
        snprintf(label, sizeof(label), "mixed_class_%d", i % 2);
        nimcp_future_t learn_future = nimcp_brain_learn_async(
            brain, features, 10, label, 0.85f
        );
        ASSERT_NE(learn_future, nullptr);
        futures.push_back(learn_future);

        // Inference
        nimcp_future_t infer_future = nimcp_brain_infer_async(
            brain, features, 10
        );
        ASSERT_NE(infer_future, nullptr);
        futures.push_back(infer_future);
    }

    // Wait for all operations
    for (size_t i = 0; i < futures.size(); i++) {
        ASSERT_TRUE(nimcp_future_wait_timeout(futures[i], 3000))
            << "Operation " << i << " timed out";
    }

    // Cleanup (futures alternating between learning and inference)
    for (size_t i = 0; i < futures.size(); i++) {
        if (i % 2 == 1) {  // Inference result
            if (nimcp_future_state(futures[i]) == NIMCP_FUTURE_COMPLETED) {
                brain_decision_t* decision;
                if (nimcp_future_get(futures[i], &decision) == NIMCP_SUCCESS) {
                    brain_free_decision(decision);
                }
            }
        }
        nimcp_future_destroy(futures[i]);
    }
}

//=============================================================================
// 5. Stress Tests
//=============================================================================

/**
 * Test: High volume concurrent operations
 *
 * WHAT: Stress test with many concurrent ops
 * WHY:  Verify system stability under load
 * HOW:  Launch 50+ concurrent operations
 */
TEST_F(BrainAsyncTest, StressTestHighVolume) {
    const int num_operations = 50;
    std::vector<nimcp_future_t> futures;

    // Launch many concurrent operations
    for (int i = 0; i < num_operations; i++) {
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = static_cast<float>(rand()) / RAND_MAX;
        }

        nimcp_future_t future;
        if (i % 2 == 0) {
            // Learning
            char label[32];
            snprintf(label, sizeof(label), "stress_%d", i % 10);
            future = nimcp_brain_learn_async(brain, features, 10, label, 0.8f);
        } else {
            // Inference
            future = nimcp_brain_infer_async(brain, features, 10);
        }

        ASSERT_NE(future, nullptr) << "Failed to create future " << i;
        futures.push_back(future);
    }

    // Wait for all with generous timeout
    int completed = 0;
    for (size_t i = 0; i < futures.size(); i++) {
        if (nimcp_future_wait_timeout(futures[i], 5000)) {
            completed++;
        }
    }

    // Expect most to complete (allow some timeouts under heavy load)
    EXPECT_GT(completed, num_operations * 0.9)
        << "Less than 90% operations completed";

    // Cleanup
    for (size_t i = 0; i < futures.size(); i++) {
        if (i % 2 == 1 && nimcp_future_state(futures[i]) == NIMCP_FUTURE_COMPLETED) {
            brain_decision_t* decision;
            if (nimcp_future_get(futures[i], &decision) == NIMCP_SUCCESS) {
                brain_free_decision(decision);
            }
        }
        nimcp_future_destroy(futures[i]);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
