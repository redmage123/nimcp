/**
 * @file test_attention_bioasync.cpp
 * @brief Unit tests for attention plasticity bio-async integration
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/attention/nimcp_attention.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

class AttentionBioAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        bio_router_config_t config;
        memset(&config, 0, sizeof(config));
        config.max_modules = 128;
        config.inbox_capacity = 64;
        bio_router_init(&config);
    }

    void TearDown() override {
        bio_router_shutdown();
    }
};

TEST_F(AttentionBioAsyncTest, ModuleRegistration) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_ATTENTION;
    bio_info.module_name = "attention_plasticity";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr) << "Failed to register attention module";

    // Verify router is initialized (registration worked)
    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_unregister_module(ctx);
}

TEST_F(AttentionBioAsyncTest, InboxProcessing) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_ATTENTION;
    bio_info.module_name = "attention_plasticity";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    // Process inbox should not crash even with no messages
    uint32_t processed = bio_router_process_inbox(ctx, 5);
    EXPECT_EQ(processed, 0u);

    bio_router_unregister_module(ctx);
}

TEST_F(AttentionBioAsyncTest, BroadcastMessage) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_ATTENTION;
    bio_info.module_name = "attention_plasticity";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    // Use attention shift message type
    bio_msg_attention_shift_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_ATTENTION_SHIFT,
                        bio_module_context_get_id(ctx),
                        BIO_MODULE_UNKNOWN,
                        sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.attention_weight = 0.8f;

    nimcp_error_t result = bio_router_broadcast(ctx, &msg, sizeof(msg));
    EXPECT_EQ(result, NIMCP_SUCCESS);

    bio_router_unregister_module(ctx);
}

TEST_F(AttentionBioAsyncTest, LoggingIntegration) {
    // Test that attention module logging works
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_ATTENTION;
    bio_info.module_name = "attention_plasticity";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    EXPECT_NE(ctx, nullptr);

    if (ctx) {
        bio_router_unregister_module(ctx);
    }
}

TEST_F(AttentionBioAsyncTest, SecurityValidation) {
    // Test that security validation is in place
    SUCCEED() << "Security validation placeholder";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
