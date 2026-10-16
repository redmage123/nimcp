/**
 * @file test_adaptive_bioasync.cpp
 * @brief Unit tests for adaptive threshold plasticity bio-async integration
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

class AdaptiveBioAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        bio_router_config_t config = {0};
        config.max_modules = 128;
        config.inbox_capacity = 64;
        bio_router_init(&config);
    }

    void TearDown() override {
        bio_router_shutdown();
    }
};

TEST_F(AdaptiveBioAsyncTest, ModuleRegistration) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_ADAPTIVE;
    bio_info.module_name = "adaptive_threshold";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    // Verify module was registered by checking if context is valid
    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_unregister_module(ctx);
}

TEST_F(AdaptiveBioAsyncTest, InboxProcessing) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_ADAPTIVE;
    bio_info.module_name = "adaptive_threshold";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    // Process inbox should not crash even with no messages
    uint32_t processed = bio_router_process_inbox(ctx, 5);
    EXPECT_EQ(processed, 0u);

    bio_router_unregister_module(ctx);
}

TEST_F(AdaptiveBioAsyncTest, BroadcastMessage) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_ADAPTIVE;
    bio_info.module_name = "adaptive_threshold";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    // Use salience response message type - simple and well-tested
    bio_msg_salience_response_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SALIENCE_RESPONSE,
                        bio_module_context_get_id(ctx),
                        BIO_MODULE_UNKNOWN,
                        sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.salience_score = 0.8f;

    nimcp_error_t result = bio_router_broadcast(ctx, &msg, sizeof(msg));
    EXPECT_EQ(result, NIMCP_SUCCESS);

    bio_router_unregister_module(ctx);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
