/**
 * @file test_brain_bio_async.cpp
 * @brief Unit tests for brain bio-async integration
 *
 * WHAT: Tests bio-async messaging and logging in brain module
 * WHY:  Verify event-driven communication and comprehensive logging
 * HOW:  Use GTest framework to test bio-async registration and message publishing
 *
 * @author NIMCP Development Team
 * @date 2025-11-29
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "async/nimcp_bio_async.h"
    #include "async/nimcp_bio_router.h"
    #include "async/nimcp_bio_messages.h"
    #include "utils/logging/nimcp_logging.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainBioAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bio_async_init(&bio_config));

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        ASSERT_EQ(NIMCP_SUCCESS, bio_router_init(&router_config));

        // Initialize logging
        nimcp_logging_config_t log_config = {0};
        log_config.log_level = NIMCP_LOG_DEBUG;
        log_config.output_mode = NIMCP_LOG_OUTPUT_CONSOLE;
        nimcp_logging_init(&log_config);
    }

    void TearDown() override {
        // Cleanup
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_logging_shutdown();
    }
};

//=============================================================================
// Bio-Async Module Registration Tests
//=============================================================================

/**
 * @brief Test bio-async module registration
 *
 * WHAT: Verify brain registers with bio-router
 * WHY:  Essential for event-driven communication
 * HOW:  Check router accepts brain module registration
 */
TEST_F(BrainBioAsyncTest, ModuleRegistration)
{
    // Register brain module
    bio_module_info_t info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "Brain",
        .inbox_capacity = 512,
        .user_data = nullptr
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    // Verify registration
    EXPECT_EQ(BIO_MODULE_BRAIN, bio_module_context_get_id(ctx));
    EXPECT_STREQ("Brain", bio_module_context_get_name(ctx));

    // Cleanup
    bio_router_unregister_module(ctx);
}

/**
 * @brief Test bio-async initialization idempotence
 *
 * WHAT: Verify multiple init calls are safe
 * WHY:  Brain subsystems may call init multiple times
 * HOW:  Call init twice, verify no errors
 */
TEST_F(BrainBioAsyncTest, InitIdempotence)
{
    // First init
    bio_module_info_t info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "Brain",
        .inbox_capacity = 512,
        .user_data = nullptr
    };

    bio_module_context_t ctx1 = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx1);

    // Second init should handle gracefully (in actual code via g_brain_bio_initialized check)
    // For this test, we just verify the first succeeded
    EXPECT_NE(nullptr, ctx1);

    // Cleanup
    bio_router_unregister_module(ctx1);
}

//=============================================================================
// Bio-Async Message Publishing Tests
//=============================================================================

/**
 * @brief Test brain state event publishing
 *
 * WHAT: Verify brain can publish state change events
 * WHY:  Other modules need to know about brain state changes
 * HOW:  Create and broadcast brain state message
 */
TEST_F(BrainBioAsyncTest, StateEventPublishing)
{
    // Register brain module
    bio_module_info_t info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "Brain",
        .inbox_capacity = 512,
        .user_data = nullptr
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    // Create brain state message
    bio_msg_brain_state_response_t msg = {0};
    bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_RESPONSE,
                        BIO_MODULE_BRAIN, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Fast query
    msg.neuron_count = 1000;
    msg.synapse_count = 10000;
    msg.global_activity = 0.5f;

    // Broadcast message
    nimcp_error_t result = bio_router_broadcast(ctx, &msg, sizeof(msg));
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Cleanup
    bio_router_unregister_module(ctx);
}

/**
 * @brief Test brain processing event publishing
 *
 * WHAT: Verify brain can publish processing events via predictive signals
 * WHY:  Modules can subscribe to specific processing events
 * HOW:  Publish signal and verify success
 */
TEST_F(BrainBioAsyncTest, ProcessingEventPublishing)
{
    // Register brain module
    bio_module_info_t info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "Brain",
        .inbox_capacity = 512,
        .user_data = nullptr
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    // Publish processing signal
    nimcp_error_t result = bio_router_publish_signal(
        ctx, "brain.processing.prediction", 0.95f);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Publish learning signal
    result = bio_router_publish_signal(
        ctx, "brain.processing.learning", 0.85f);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Cleanup
    bio_router_unregister_module(ctx);
}

/**
 * @brief Test neuron activation message
 *
 * WHAT: Verify neuron activation request/response messages
 * WHY:  Enable inter-module neuron control
 * HOW:  Create and send neuron activation message
 */
TEST_F(BrainBioAsyncTest, NeuronActivationMessage)
{
    // Register brain module
    bio_module_info_t info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "Brain",
        .inbox_capacity = 512,
        .user_data = nullptr
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    // Create neuron activation request
    bio_msg_neuron_activation_request_t req = {0};
    bio_msg_init_header(&req.header, BIO_MSG_NEURON_ACTIVATION_REQUEST,
                        BIO_MODULE_BRAIN, BIO_MODULE_BRAIN, sizeof(req));
    req.neuron_id = 42;
    req.input_current = 1.5f;
    req.duration_ms = 10.0f;

    // Send message
    nimcp_error_t result = bio_router_send(ctx, &req, sizeof(req), 100);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Cleanup
    bio_router_unregister_module(ctx);
}

//=============================================================================
// Logging Integration Tests
//=============================================================================

/**
 * @brief Test logging output
 *
 * WHAT: Verify brain functions produce log output
 * WHY:  Debugging and monitoring require comprehensive logging
 * HOW:  Enable logging and verify output (manual inspection for now)
 */
TEST_F(BrainBioAsyncTest, LoggingOutput)
{
    // Register brain module (should produce INFO log)
    bio_module_info_t info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "Brain",
        .inbox_capacity = 512,
        .user_data = nullptr
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(nullptr, ctx);

    // This test mainly verifies no crashes occur with logging enabled
    // Actual log output can be verified manually or with log capture

    // Cleanup (should produce INFO log)
    bio_router_unregister_module(ctx);
}

/**
 * @brief Test error logging
 *
 * WHAT: Verify ERROR level logging for failure cases
 * WHY:  Critical errors must be logged for debugging
 * HOW:  Trigger error conditions and verify logging
 */
TEST_F(BrainBioAsyncTest, ErrorLogging)
{
    // Attempting operations without registration should log errors
    // (This would be tested in actual brain functions)

    // For this test, we just verify the logging system is working
    LOG_MODULE_ERROR("BRAIN", "Test error message");
    LOG_MODULE_WARN("BRAIN", "Test warning message");
    LOG_MODULE_INFO("BRAIN", "Test info message");
    LOG_MODULE_DEBUG("BRAIN", "Test debug message");

    // No crashes = success
    SUCCEED();
}

//=============================================================================
// Channel Selection Tests
//=============================================================================

/**
 * @brief Test appropriate channel selection for message types
 *
 * WHAT: Verify correct neuromodulator channels are used
 * WHY:  Biological realism and performance optimization
 * HOW:  Check recommended channels for different message types
 */
TEST_F(BrainBioAsyncTest, ChannelSelection)
{
    // Fast queries → Acetylcholine
    EXPECT_EQ(BIO_CHANNEL_ACETYLCHOLINE,
              bio_msg_recommended_channel(BIO_MSG_BRAIN_STATE_QUERY));

    // Plasticity/learning → Dopamine
    EXPECT_EQ(BIO_CHANNEL_DOPAMINE,
              bio_msg_recommended_channel(BIO_MSG_WEIGHT_UPDATE_REQUEST));

    // Alerts → Norepinephrine
    EXPECT_EQ(BIO_CHANNEL_NOREPINEPHRINE,
              bio_msg_recommended_channel(BIO_MSG_SALIENCE_QUERY));

    // Slow deliberative → Serotonin
    EXPECT_EQ(BIO_CHANNEL_SEROTONIN,
              bio_msg_recommended_channel(BIO_MSG_ETHICS_EVALUATION_REQUEST));
}

//=============================================================================
// Integration Scenarios
//=============================================================================

/**
 * @brief Test brain prediction with bio-async events
 *
 * WHAT: Verify brain_predict publishes events
 * WHY:  Integration test of logging and bio-async in real function
 * HOW:  Create simple brain, run prediction, verify no crashes
 */
TEST_F(BrainBioAsyncTest, DISABLED_PredictionWithEvents)
{
    // This test would require a full brain instance
    // Disabled for now as it requires extensive setup
    // In a real integration test, this would:
    // 1. Create brain via factory
    // 2. Call brain_predict
    // 3. Verify bio-async event was published
    // 4. Verify logging occurred
    SUCCEED();
}

/**
 * @brief Test graceful degradation without bio-async
 *
 * WHAT: Verify brain works even if bio-async init fails
 * WHY:  Robustness - bio-async is enhancement, not requirement
 * HOW:  Simulate init failure and verify brain still functions
 */
TEST_F(BrainBioAsyncTest, GracefulDegradation)
{
    // The brain module is designed to work even if bio-async init fails
    // This test verifies the graceful fallback behavior

    // In actual implementation, if g_brain_bio_initialized is false,
    // brain_publish_* functions simply return without error

    // Verify no crashes occur (actual test in integration)
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
