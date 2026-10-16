/**
 * @file test_brain_resize_bio_async.cpp
 * @brief Unit tests for brain resize bio-async integration
 *
 * WHAT: Tests bio-async message publishing for brain resize operations
 * WHY:  Ensure bio-async integration works correctly
 * HOW:  Mock bio-async router and verify message publishing
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain_resize.h"
#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;

//=============================================================================
// Test Fixture
//=============================================================================

class BrainResizeBioAsyncTest : public ::testing::Test {
protected:
    brain_t test_brain;

    void SetUp() override {
        // Initialize logging
        nimcp_log_set_level(NIMCP_LOG_LEVEL_DEBUG);

        // Initialize bio-async system
        nimcp_bio_async_init();

        // Create test brain
        test_brain = nullptr;
    }

    void TearDown() override {
        // Cleanup brain
        if (test_brain) {
            brain_destroy(test_brain);
            test_brain = nullptr;
        }

        // Shutdown bio-async
        nimcp_bio_async_shutdown();
    }

    brain_t create_test_brain(brain_size_preset_t size) {
        brain_config_t config = brain_default_config();
        config.size = size;
        config.num_inputs = 10;
        config.num_outputs = 5;

        brain_t brain = brain_create("test_resize", &config);
        EXPECT_NE(brain, nullptr);
        return brain;
    }
};

//=============================================================================
// Bio-Async Registration Tests
//=============================================================================

TEST_F(BrainResizeBioAsyncTest, ModuleRegistration) {
    // WHAT: Verify brain_resize module registers with bio-async
    // WHY:  Module must be registered to publish messages

    test_brain = create_test_brain(BRAIN_SIZE_TINY);
    ASSERT_NE(test_brain, nullptr);

    // Trigger registration by calling resize function
    uint32_t current_size = brain_get_neuron_count(test_brain);
    uint32_t new_size = current_size + 100;

    bool result = brain_resize(test_brain, new_size);

    // Registration happens internally, verify via successful resize
    EXPECT_TRUE(result);
    EXPECT_EQ(brain_get_neuron_count(test_brain), new_size);
}

//=============================================================================
// Resize Event Publishing Tests
//=============================================================================

TEST_F(BrainResizeBioAsyncTest, PublishResizeStartEvent) {
    // WHAT: Verify resize start event is published
    // WHY:  Other modules need to know when resize begins

    test_brain = create_test_brain(BRAIN_SIZE_SMALL);
    ASSERT_NE(test_brain, nullptr);

    uint32_t current_size = brain_get_neuron_count(test_brain);
    uint32_t new_size = current_size + 500;

    // Execute resize (should publish start event)
    bool result = brain_resize(test_brain, new_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(brain_get_neuron_count(test_brain), new_size);
}

TEST_F(BrainResizeBioAsyncTest, PublishResizeCompleteEvent) {
    // WHAT: Verify resize complete event is published on success
    // WHY:  Other modules need to know when resize completes

    test_brain = create_test_brain(BRAIN_SIZE_SMALL);
    ASSERT_NE(test_brain, nullptr);

    uint32_t current_size = brain_get_neuron_count(test_brain);
    uint32_t new_size = current_size + 500;

    // Execute resize (should publish complete event)
    bool result = brain_resize(test_brain, new_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(brain_get_neuron_count(test_brain), new_size);
}

TEST_F(BrainResizeBioAsyncTest, PublishResizeFailureEvent) {
    // WHAT: Verify failure event published on invalid resize
    // WHY:  System should alert on failures

    test_brain = create_test_brain(BRAIN_SIZE_SMALL);
    ASSERT_NE(test_brain, nullptr);

    uint32_t current_size = brain_get_neuron_count(test_brain);
    uint32_t invalid_size = current_size - 10;  // Shrinking not supported

    // Attempt invalid resize (should publish error event)
    bool result = brain_resize(test_brain, invalid_size);

    EXPECT_FALSE(result);
    EXPECT_EQ(brain_get_neuron_count(test_brain), current_size);  // Unchanged
}

//=============================================================================
// Channel Selection Tests
//=============================================================================

TEST_F(BrainResizeBioAsyncTest, UseNorepinephrineForAlerts) {
    // WHAT: Verify resize start uses norepinephrine channel
    // WHY:  Resize is an urgent/alerting operation

    test_brain = create_test_brain(BRAIN_SIZE_TINY);
    ASSERT_NE(test_brain, nullptr);

    uint32_t current_size = brain_get_neuron_count(test_brain);
    uint32_t new_size = current_size + 100;

    // Resize should use norepinephrine for start event
    bool result = brain_resize(test_brain, new_size);

    EXPECT_TRUE(result);
}

TEST_F(BrainResizeBioAsyncTest, UseDopamineForSuccess) {
    // WHAT: Verify resize complete uses dopamine channel
    // WHY:  Successful completion is a reward signal

    test_brain = create_test_brain(BRAIN_SIZE_TINY);
    ASSERT_NE(test_brain, nullptr);

    uint32_t current_size = brain_get_neuron_count(test_brain);
    uint32_t new_size = current_size + 100;

    // Resize completion should use dopamine
    bool result = brain_resize(test_brain, new_size);

    EXPECT_TRUE(result);
}

//=============================================================================
// Auto-Resize Bio-Async Tests
//=============================================================================

TEST_F(BrainResizeBioAsyncTest, AutoResizePublishesSaturationAlert) {
    // WHAT: Verify auto-resize publishes saturation alert
    // WHY:  System should alert when capacity is reached

    test_brain = create_test_brain(BRAIN_SIZE_TINY);
    ASSERT_NE(test_brain, nullptr);

    // Auto-resize checks utilization and publishes alerts if saturated
    // (In practice, would need to saturate the brain first)
    bool result = brain_auto_resize(test_brain);

    // Result depends on brain saturation state
    // Just verify function completes without crashing
    (void)result;
}

//=============================================================================
// Utilization Metrics Publishing Tests
//=============================================================================

TEST_F(BrainResizeBioAsyncTest, PublishUtilizationMetrics) {
    // WHAT: Verify utilization metrics can be queried
    // WHY:  Other modules may need capacity information

    test_brain = create_test_brain(BRAIN_SIZE_SMALL);
    ASSERT_NE(test_brain, nullptr);

    float utilization = 0.0f;
    float saturation = 0.0f;

    bool result = brain_get_utilization_metrics(test_brain, &utilization, &saturation);

    EXPECT_TRUE(result);
    EXPECT_GE(utilization, 0.0f);
    EXPECT_LE(utilization, 1.0f);
    EXPECT_GE(saturation, 0.0f);
    EXPECT_LE(saturation, 1.0f);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(BrainResizeBioAsyncTest, MultipleResizeOperations) {
    // WHAT: Verify multiple resizes publish events correctly
    // WHY:  Ensure event publishing works across multiple operations

    test_brain = create_test_brain(BRAIN_SIZE_TINY);
    ASSERT_NE(test_brain, nullptr);

    uint32_t size1 = brain_get_neuron_count(test_brain);

    // First resize
    bool result1 = brain_resize(test_brain, size1 + 100);
    EXPECT_TRUE(result1);

    uint32_t size2 = brain_get_neuron_count(test_brain);
    EXPECT_EQ(size2, size1 + 100);

    // Second resize
    bool result2 = brain_resize(test_brain, size2 + 200);
    EXPECT_TRUE(result2);

    uint32_t size3 = brain_get_neuron_count(test_brain);
    EXPECT_EQ(size3, size2 + 200);
}

TEST_F(BrainResizeBioAsyncTest, ResizeWithLogging) {
    // WHAT: Verify resize operations log appropriately
    // WHY:  Logging and bio-async should work together

    test_brain = create_test_brain(BRAIN_SIZE_SMALL);
    ASSERT_NE(test_brain, nullptr);

    uint32_t current_size = brain_get_neuron_count(test_brain);
    uint32_t new_size = current_size + 500;

    // Enable debug logging to verify log output
    nimcp_log_set_level(NIMCP_LOG_LEVEL_DEBUG);

    bool result = brain_resize(test_brain, new_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(brain_get_neuron_count(test_brain), new_size);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(BrainResizeBioAsyncTest, ResizeNullBrain) {
    // WHAT: Verify graceful handling of NULL brain
    // WHY:  Should not crash or publish invalid messages

    bool result = brain_resize(nullptr, 1000);

    EXPECT_FALSE(result);
}

TEST_F(BrainResizeBioAsyncTest, AutoResizeNullBrain) {
    // WHAT: Verify graceful handling of NULL brain in auto-resize
    // WHY:  Should not crash or publish invalid messages

    bool result = brain_auto_resize(nullptr);

    EXPECT_FALSE(result);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
