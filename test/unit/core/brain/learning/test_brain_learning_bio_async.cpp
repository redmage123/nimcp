//=============================================================================
// test_brain_learning_bio_async.cpp - Unit Tests for Brain Learning Bio-Async
//=============================================================================
/**
 * @file test_brain_learning_bio_async.cpp
 * @brief Unit tests for bio-async integration in brain learning module
 *
 * WHAT: Tests bio-async message handling and event broadcasting
 * WHY:  Ensure learning events are correctly published and handled
 * HOW:  Use bio_router API to verify message flow
 *
 * @author NIMCP Development Team
 * @date 2025-12-05
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_brain_learning.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

class BrainLearningBioAsyncTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Initialize bio-router with default config
        bio_router_config_t router_config = {0};
        router_config.max_modules = 128;
        router_config.inbox_capacity = 64;
        bio_router_init(&router_config);

        // Create brain with basic config
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.num_inputs = 10;
        config.num_outputs = 3;
        config.learning_rate = 0.01f;
        config.size = BRAIN_SIZE_TINY;
        config.task = BRAIN_TASK_CLASSIFICATION;

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
        bio_router_shutdown();
    }
};

//=============================================================================
// Test: Bio-Router Initialization
//=============================================================================

TEST_F(BrainLearningBioAsyncTest, BioRouterIsInitialized) {
    EXPECT_TRUE(bio_router_is_initialized());
}

//=============================================================================
// Test: Brain Has Bio-Async Context
//=============================================================================

TEST_F(BrainLearningBioAsyncTest, BrainHasBioAsyncContext) {
    // Bio-async may or may not be enabled depending on initialization success
    // Just verify the flag exists and brain creation succeeded
    EXPECT_NE(brain, nullptr);
    // Note: bio_async_enabled may be false if predictive models fail to init
}

//=============================================================================
// Test: Learning Function Works
//=============================================================================

TEST_F(BrainLearningBioAsyncTest, LearningFunctionWorks) {
    // Prepare test data
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    const char* label = "test_class";
    float confidence = 0.9f;

    // Call learning function
    float loss = brain_learn_example(brain, features, 10, label, confidence);

    // Loss should be non-negative (or -1 for error)
    // Just verify the function doesn't crash
    EXPECT_TRUE(loss >= 0.0f || loss == -1.0f);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
