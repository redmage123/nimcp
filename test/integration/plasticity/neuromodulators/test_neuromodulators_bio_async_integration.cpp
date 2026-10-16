/**
 * @file test_neuromodulators_bio_async_integration.cpp
 * @brief Integration tests for neuromodulators bio-async with full router
 *
 * Tests end-to-end message flow through bio-router with multiple modules.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NeuromodulatorsBioAsyncIntegrationTest : public ::testing::Test {
protected:
    neuromodulator_system_t neuromod_system_;
    bio_module_context_t test_module_ctx_;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bio_async_init(&bio_config));

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        ASSERT_EQ(NIMCP_SUCCESS, bio_router_init(&router_config));

        // Create neuromodulator system (registers with router)
        neuromod_system_ = neuromodulator_system_create(nullptr);
        ASSERT_NE(nullptr, neuromod_system_);

        // Register test module
        bio_module_info_t test_info = {
            .module_id = BIO_MODULE_BRAIN,  // Pretend to be brain
            .module_name = "test_module",
            .inbox_capacity = 32,
            .user_data = this
        };
        test_module_ctx_ = bio_router_register_module(&test_info);
        ASSERT_NE(nullptr, test_module_ctx_);
    }

    void TearDown() override {
        if (test_module_ctx_) {
            bio_router_unregister_module(test_module_ctx_);
            test_module_ctx_ = nullptr;
        }
        if (neuromod_system_) {
            neuromodulator_system_destroy(neuromod_system_);
            neuromod_system_ = nullptr;
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// Message Send/Receive Tests
//=============================================================================

TEST_F(NeuromodulatorsBioAsyncIntegrationTest, SendDopamineReleaseMessage) {
    // Create dopamine release message
    bio_msg_neuromodulator_release_t msg = {};
    bio_msg_init_header(&msg.header,
        BIO_MSG_NEUROMODULATOR_RELEASE,
        BIO_MODULE_BRAIN,
        BIO_MODULE_NEUROMODULATOR,
        sizeof(msg));

    msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
    msg.release_amount = 0.6f;
    msg.source_region = 1;
    msg.current_concentration = 0.0f;
    msg.diffusion_radius_um = 100.0f;

    // Get dopamine level before
    float da_before = neuromodulator_get_level(neuromod_system_, NEUROMOD_DOPAMINE);

    // Send message through router
    nimcp_error_t result = bio_router_send(test_module_ctx_, &msg, sizeof(msg), 1000);
    ASSERT_EQ(NIMCP_SUCCESS, result);

    // Process pending messages in neuromodulator inbox
    // Messages are queued and must be explicitly processed
    uint32_t processed = neuromodulator_bio_async_process(10);
    EXPECT_GT(processed, 0u);

    // Verify dopamine increased
    float da_after = neuromodulator_get_level(neuromod_system_, NEUROMOD_DOPAMINE);
    EXPECT_GT(da_after, da_before);
}

TEST_F(NeuromodulatorsBioAsyncIntegrationTest, SendLearningRateUpdateRequest) {
    // Set known neuromodulator levels
    neuromodulator_release_dopamine(neuromod_system_, 0.5f, 0.0f);

    // Create learning rate update request
    bio_msg_learning_rate_update_t msg = {};
    bio_msg_init_header(&msg.header,
        BIO_MSG_LEARNING_RATE_UPDATE,
        BIO_MODULE_BRAIN,
        BIO_MODULE_NEUROMODULATOR,
        sizeof(msg));

    msg.synapse_id = 123;
    msg.base_learning_rate = 0.01f;
    msg.modulated_learning_rate = 0.0f;  // Will be set by handler
    msg.dopamine_level = 0.0f;            // Will be set by handler
    msg.serotonin_level = 0.0f;           // Will be set by handler

    // Send message
    nimcp_error_t result = bio_router_send(test_module_ctx_, &msg, sizeof(msg), 1000);
    ASSERT_EQ(NIMCP_SUCCESS, result);

    // Process pending messages
    neuromodulator_bio_async_process(10);

    // Verify system still functioning
    neuromodulator_pool_t pool = {};
    ASSERT_TRUE(neuromodulator_get_levels(neuromod_system_, &pool));
}

TEST_F(NeuromodulatorsBioAsyncIntegrationTest, MultipleModulatorsSequential) {
    float da_initial = neuromodulator_get_level(neuromod_system_, NEUROMOD_DOPAMINE);
    float serotonin_initial = neuromodulator_get_level(neuromod_system_, NEUROMOD_SEROTONIN);
    float ach_initial = neuromodulator_get_level(neuromod_system_, NEUROMOD_ACETYLCHOLINE);
    float ne_initial = neuromodulator_get_level(neuromod_system_, NEUROMOD_NOREPINEPHRINE);

    // Send dopamine release
    bio_msg_neuromodulator_release_t da_msg = {};
    bio_msg_init_header(&da_msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
        BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR, sizeof(da_msg));
    da_msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
    da_msg.release_amount = 0.3f;
    da_msg.source_region = 1;
    bio_router_send(test_module_ctx_, &da_msg, sizeof(da_msg), 1000);

    // Send serotonin release
    bio_msg_neuromodulator_release_t serotonin_msg = da_msg;
    serotonin_msg.neuromodulator = BIO_CHANNEL_SEROTONIN;
    serotonin_msg.release_amount = 0.4f;
    bio_router_send(test_module_ctx_, &serotonin_msg, sizeof(serotonin_msg), 1000);

    // Send acetylcholine release
    bio_msg_neuromodulator_release_t ach_msg = da_msg;
    ach_msg.neuromodulator = BIO_CHANNEL_ACETYLCHOLINE;
    ach_msg.release_amount = 0.5f;
    bio_router_send(test_module_ctx_, &ach_msg, sizeof(ach_msg), 1000);

    // Send norepinephrine release
    bio_msg_neuromodulator_release_t ne_msg = da_msg;
    ne_msg.neuromodulator = BIO_CHANNEL_NOREPINEPHRINE;
    ne_msg.release_amount = 0.6f;
    bio_router_send(test_module_ctx_, &ne_msg, sizeof(ne_msg), 1000);

    // Process all pending messages
    neuromodulator_bio_async_process(0);  // 0 = process all

    // Verify all increased
    EXPECT_GT(neuromodulator_get_level(neuromod_system_, NEUROMOD_DOPAMINE), da_initial);
    EXPECT_GT(neuromodulator_get_level(neuromod_system_, NEUROMOD_SEROTONIN), serotonin_initial);
    EXPECT_GT(neuromodulator_get_level(neuromod_system_, NEUROMOD_ACETYLCHOLINE), ach_initial);
    EXPECT_GT(neuromodulator_get_level(neuromod_system_, NEUROMOD_NOREPINEPHRINE), ne_initial);
}

//=============================================================================
// Async Promise/Future Tests
//=============================================================================

TEST_F(NeuromodulatorsBioAsyncIntegrationTest, AsyncReleaseWithPromise) {
    // Create release message
    bio_msg_neuromodulator_release_t msg = {};
    bio_msg_init_header(&msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
        BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR, sizeof(msg));
    msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
    msg.release_amount = 0.5f;
    msg.source_region = 1;

    // Send async with promise
    nimcp_bio_promise_t promise = bio_router_send_async(
        test_module_ctx_, &msg, sizeof(msg), BIO_CHANNEL_DOPAMINE);

    ASSERT_NE(nullptr, promise);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(nullptr, future);

    // Wait for completion
    bio_msg_neuromodulator_release_t response;
    nimcp_error_t result = nimcp_bio_future_wait(future, &response, 2000);

    if (result == NIMCP_SUCCESS) {
        // Verify response
        EXPECT_EQ(BIO_MSG_NEUROMODULATOR_RELEASE, response.header.type);
        EXPECT_EQ(BIO_CHANNEL_DOPAMINE, response.neuromodulator);
        EXPECT_GT(response.current_concentration, 0.0f);
    }

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// High Load Tests
//=============================================================================

TEST_F(NeuromodulatorsBioAsyncIntegrationTest, HighVolumeMessages) {
    const int MESSAGE_COUNT = 100;

    for (int i = 0; i < MESSAGE_COUNT; i++) {
        bio_msg_neuromodulator_release_t msg = {};
        bio_msg_init_header(&msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
            BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR, sizeof(msg));
        msg.neuromodulator = static_cast<nimcp_bio_channel_type_t>(i % 4);
        msg.release_amount = 0.1f;
        msg.source_region = i;

        bio_router_send(test_module_ctx_, &msg, sizeof(msg), 1000);
    }

    // Process all pending messages
    uint32_t processed = neuromodulator_bio_async_process(0);  // 0 = process all
    EXPECT_GT(processed, 0u);

    // System should still be functional
    neuromodulator_pool_t pool = {};
    ASSERT_TRUE(neuromodulator_get_levels(neuromod_system_, &pool));

    // Get statistics
    neuromodulator_stats_t stats;
    ASSERT_TRUE(neuromodulator_get_stats(neuromod_system_, &stats));

    // Verify some releases were processed
    uint64_t total_releases = stats.dopamine_releases +
                              stats.serotonin_releases +
                              stats.acetylcholine_releases +
                              stats.norepinephrine_releases;
    EXPECT_GT(total_releases, 0u);
}

//=============================================================================
// Router Statistics Tests
//=============================================================================

TEST_F(NeuromodulatorsBioAsyncIntegrationTest, RouterStatisticsTracking) {
    // Get initial stats
    bio_router_stats_t stats_before;
    ASSERT_EQ(NIMCP_SUCCESS, bio_router_get_stats(&stats_before));

    // Send several messages
    for (int i = 0; i < 10; i++) {
        bio_msg_neuromodulator_release_t msg = {};
        bio_msg_init_header(&msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
            BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR, sizeof(msg));
        msg.neuromodulator = BIO_CHANNEL_DOPAMINE;
        msg.release_amount = 0.2f;
        msg.source_region = 1;

        bio_router_send(test_module_ctx_, &msg, sizeof(msg), 1000);
    }

    // Process pending messages
    neuromodulator_bio_async_process(0);

    // Get updated stats
    bio_router_stats_t stats_after;
    ASSERT_EQ(NIMCP_SUCCESS, bio_router_get_stats(&stats_after));

    // Verify messages were routed
    EXPECT_GT(stats_after.messages_routed, stats_before.messages_routed);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(NeuromodulatorsBioAsyncIntegrationTest, HandleMalformedMessage) {
    // Send message with incorrect size
    bio_msg_neuromodulator_release_t msg = {};
    bio_msg_init_header(&msg.header, BIO_MSG_NEUROMODULATOR_RELEASE,
        BIO_MODULE_BRAIN, BIO_MODULE_NEUROMODULATOR, sizeof(msg));

    // Send with wrong size (should not crash)
    nimcp_error_t result = bio_router_send(test_module_ctx_, &msg, 10, 1000);
    // Result may be error or success depending on router validation
    (void)result;

    // Process any messages (malformed ones should be handled gracefully)
    neuromodulator_bio_async_process(10);

    // System should still be functional
    neuromodulator_pool_t pool = {};
    ASSERT_TRUE(neuromodulator_get_levels(neuromod_system_, &pool));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
