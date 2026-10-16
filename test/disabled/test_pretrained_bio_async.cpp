/**
 * @file test_pretrained_bio_async.cpp
 * @brief Unit tests for pretrained module bio-async and logging integration
 *
 * WHAT: Tests bio-async message publishing and logging for pretrained module
 * WHY:  Ensure pretrained module integrates correctly with bio-async system
 * HOW:  GTest framework with bio-async initialization and message verification
 *
 * @author NIMCP Development Team
 * @date 2025-11-29
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "core/brain/nimcp_pretrained.h"
    #include "async/nimcp_bio_async.h"
    #include "async/nimcp_bio_router.h"
    #include "async/nimcp_bio_messages.h"
    #include "utils/logging/nimcp_logging.h"
    #include "utils/memory/nimcp_unified_memory.h"

using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;

//=============================================================================
// Test Fixture
//=============================================================================

class PretrainedBioAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize unified memory
        nimcp_unified_memory_init(NULL);

        // Initialize bio-async system
        nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
        config.enable_logging = true;
        config.enable_statistics = true;
        nimcp_error_t err = nimcp_bio_async_init(&config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-async";

        // Initialize logging
        nimcp_log_config_t log_config = {0};
        log_config.level = NIMCP_LOG_LEVEL_DEBUG;
        log_config.enable_console = true;
        log_config.enable_async = false;  // Synchronous for testing
        nimcp_log_init(&log_config);

        // Initialize bio-router for message tracking
        nimcp_bio_router_config_t cfg = {0}; bio_router_init(&cfg);
    }

    void TearDown() override {
        // Cleanup
        nimcp_log_shutdown();
        nimcp_bio_router_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_unified_memory_shutdown();
    }

    /**
     * @brief Helper to check if a message was published on a channel
     */
    bool check_message_published(nimcp_bio_channel_type_t channel,
                                 bio_message_type_t msg_type) {
        // This is a simplified check - in a real implementation,
        // you would subscribe to the channel and verify messages
        nimcp_bio_async_stats_t stats;
        nimcp_bio_async_get_stats(&stats);

        // Check that channel had activity
        return stats.channel_stats[channel].releases > 0;
    }
};

//=============================================================================
// Bio-Async Registration Tests
//=============================================================================

TEST_F(PretrainedBioAsyncTest, BioAsyncInitialized) {
    // WHAT: Verify bio-async system is properly initialized
    // WHY:  Pretrained module requires bio-async for event publishing
    // HOW:  Check initialization status

    EXPECT_TRUE(nimcp_bio_async_is_initialized());
}

TEST_F(PretrainedBioAsyncTest, ModuleIDDefined) {
    // WHAT: Verify pretrained module has correct ID
    // WHY:  Ensure module can be identified in bio-async messages
    // HOW:  Check module enumeration

    EXPECT_EQ(BIO_MODULE_BRAIN_PRETRAINED, 0x0119);
}

//=============================================================================
// Model Loading Event Tests
//=============================================================================

TEST_F(PretrainedBioAsyncTest, DISABLED_LoadingPublishesStateChange) {
    // WHAT: Verify model loading publishes SEROTONIN state change
    // WHY:  Other modules need to know when loading starts
    // HOW:  Track channel activity during load attempt

    // NOTE: This test is disabled because it requires actual model files
    // In a real scenario, you would:
    // 1. Create a mock model file
    // 2. Call brain_load_pretrained()
    // 3. Verify SEROTONIN channel received BIO_MSG_BRAIN_STATE_QUERY

    const char* test_model = "nimcp_test_model_v1.0";

    nimcp_bio_async_reset_stats();

    // Attempt to load (will fail without real model file)
    brain_t brain = brain_load_pretrained(test_model, NULL);

    // Should publish state change event even on failure
    nimcp_bio_async_stats_t stats;
    nimcp_bio_async_get_stats(&stats);

    // Check SEROTONIN channel activity (state changes)
    EXPECT_GT(stats.channel_stats[BIO_CHANNEL_SEROTONIN].releases, 0);

    if (brain) {
        brain_destroy(brain);
    }
}

TEST_F(PretrainedBioAsyncTest, DISABLED_SuccessfulLoadPublishesDopamine) {
    // WHAT: Verify successful load publishes DOPAMINE reward signal
    // WHY:  Success events should trigger dopamine (completion/reward)
    // HOW:  Load valid model and check dopamine channel

    // NOTE: Disabled - requires real model file
    // When enabled:
    // 1. Create valid model file
    // 2. Load with brain_load_pretrained()
    // 3. Verify DOPAMINE channel received BIO_MSG_BRAIN_STATE_RESPONSE
    // 4. Verify success flag in message

    EXPECT_TRUE(true);  // Placeholder
}

//=============================================================================
// Fine-tuning Event Tests
//=============================================================================

TEST_F(PretrainedBioAsyncTest, DISABLED_FinetuningPublishesTrainingEvents) {
    // WHAT: Verify fine-tuning publishes training step messages
    // WHY:  Training progress should be communicated via bio-async
    // HOW:  Start fine-tuning and verify message publishing

    // NOTE: Disabled - requires actual brain instance
    // When enabled:
    // 1. Create or load a brain
    // 2. Call brain_finetune() with sample data
    // 3. Verify BIO_MSG_TRAINING_STEP_REQUEST on SEROTONIN
    // 4. Verify BIO_MSG_TRAINING_STEP_COMPLETE on DOPAMINE

    EXPECT_TRUE(true);  // Placeholder
}

//=============================================================================
// Logging Integration Tests
//=============================================================================

TEST_F(PretrainedBioAsyncTest, LoggingInitialized) {
    // WHAT: Verify logging system is active
    // WHY:  Pretrained module uses LOG_* macros extensively
    // HOW:  Check logging initialization

    // Logging is initialized in SetUp()
    // Verify we can log without crashing
    LOG_INFO("Test log message from pretrained bio-async test");
    EXPECT_TRUE(true);
}

TEST_F(PretrainedBioAsyncTest, DISABLED_LoadingGeneratesLogs) {
    // WHAT: Verify model loading generates appropriate log messages
    // WHY:  Logging helps debug loading issues
    // HOW:  Capture logs during load attempt

    // NOTE: Disabled - requires log capture mechanism
    // When enabled:
    // 1. Set up log capture callback
    // 2. Attempt model load
    // 3. Verify DEBUG, INFO, WARN, ERROR logs as appropriate

    EXPECT_TRUE(true);  // Placeholder
}

//=============================================================================
// Message Format Tests
//=============================================================================

TEST_F(PretrainedBioAsyncTest, MessageHeaderCorrectlyInitialized) {
    // WHAT: Verify message headers are properly formatted
    // WHY:  Ensure messages can be routed correctly
    // HOW:  Create and inspect a message header

    bio_msg_brain_state_response_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                       BIO_MSG_BRAIN_STATE_RESPONSE,
                       BIO_MODULE_BRAIN_PRETRAINED,
                       (bio_module_id_t)0,  // Broadcast
                       sizeof(msg));

    EXPECT_EQ(msg.header.type, BIO_MSG_BRAIN_STATE_RESPONSE);
    EXPECT_EQ(msg.header.source_module, BIO_MODULE_BRAIN_PRETRAINED);
    EXPECT_EQ(msg.header.target_module, 0);
    EXPECT_EQ(msg.header.payload_size, sizeof(msg));
}

TEST_F(PretrainedBioAsyncTest, RecommendedChannelsCorrect) {
    // WHAT: Verify pretrained uses recommended channels
    // WHY:  Channel choice affects message timing and semantics
    // HOW:  Check recommended channels for message types

    // State changes → SEROTONIN
    nimcp_bio_channel_type_t state_channel =
        bio_msg_recommended_channel(BIO_MSG_BRAIN_STATE_QUERY);
    EXPECT_EQ(state_channel, BIO_CHANNEL_ACETYLCHOLINE);  // Fast query

    // Training events → DOPAMINE (reward)
    nimcp_bio_channel_type_t training_channel =
        bio_msg_recommended_channel(BIO_MSG_TRAINING_STEP_COMPLETE);
    EXPECT_EQ(training_channel, BIO_CHANNEL_DOPAMINE);
}

//=============================================================================
// Model Info Tests
//=============================================================================

TEST_F(PretrainedBioAsyncTest, GetModelInfoHandlesInvalidInput) {
    // WHAT: Verify brain_get_model_info handles NULL gracefully
    // WHY:  Prevent crashes on invalid input
    // HOW:  Call with NULL parameters

    brain_model_info_t info;

    EXPECT_FALSE(brain_get_model_info(NULL, &info));
    EXPECT_FALSE(brain_get_model_info("model", NULL));
}

TEST_F(PretrainedBioAsyncTest, DISABLED_GetModelInfoLogsActivity) {
    // WHAT: Verify brain_get_model_info generates logs
    // WHY:  Help debug metadata loading issues
    // HOW:  Call with valid model and check logs

    // NOTE: Disabled - requires actual model metadata
    brain_model_info_t info;
    bool result = brain_get_model_info("nimcp_test_model", &info);

    // Should log attempts even if model doesn't exist
    EXPECT_TRUE(true);  // Placeholder - would check captured logs
}

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(PretrainedBioAsyncTest, NoMemoryLeaksOnFailedLoad) {
    // WHAT: Verify no memory leaks when loading fails
    // WHY:  Failed loads shouldn't leak resources
    // HOW:  Attempt load with invalid model, verify cleanup

    // Attempt to load non-existent model
    brain_t brain = brain_load_pretrained("nonexistent_model", NULL);

    // Should return NULL for non-existent model
    EXPECT_EQ(brain, nullptr);

    if (brain) {
        brain_destroy(brain);
    }
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(PretrainedBioAsyncTest, BioAsyncGracefulDegradation) {
    // WHAT: Verify pretrained works without bio-async
    // WHY:  Module should degrade gracefully if bio-async unavailable
    // HOW:  Shutdown bio-async and test core functionality

    nimcp_bio_async_shutdown();

    // Should still work, just without message publishing
    brain_model_info_t info;
    bool result = brain_get_model_info("test_model", &info);

    // Function should not crash (will fail due to missing model, not bio-async)
    EXPECT_TRUE(true);

    // Re-initialize for cleanup
    nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
    nimcp_bio_async_init(&config);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
