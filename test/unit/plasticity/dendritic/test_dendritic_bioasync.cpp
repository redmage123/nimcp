/**
 * @file test_dendritic_bioasync.cpp
 * @brief Unit tests for dendritic plasticity bio-async integration
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

class DendriticBioAsyncTest : public ::testing::Test {
protected:
    dendritic_tree_t tree;

    void SetUp() override {
        bio_router_config_t config;
        memset(&config, 0, sizeof(config));
        config.max_modules = 128;
        config.inbox_capacity = 64;
        bio_router_init(&config);

        // Create test dendritic tree
        dendritic_tree_config_t tree_config = dendritic_tree_config_default();
        tree = dendritic_tree_create(&tree_config);
        ASSERT_NE(tree, nullptr);
    }

    void TearDown() override {
        if (tree) {
            dendritic_tree_destroy(tree);
        }
        bio_router_shutdown();
    }
};

TEST_F(DendriticBioAsyncTest, ModuleRegistration) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_DENDRITIC;
    bio_info.module_name = "dendritic_plasticity";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_unregister_module(ctx);
}

TEST_F(DendriticBioAsyncTest, InboxProcessing) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_DENDRITIC;
    bio_info.module_name = "dendritic_plasticity";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    uint32_t processed = bio_router_process_inbox(ctx, 5);
    EXPECT_EQ(processed, 0u);

    bio_router_unregister_module(ctx);
}

TEST_F(DendriticBioAsyncTest, BroadcastMessage) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_DENDRITIC;
    bio_info.module_name = "dendritic_plasticity";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    // Use salience response message type
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

TEST_F(DendriticBioAsyncTest, CalciumConcentrationValidation) {
    // Test that calcium concentrations are validated
    float total_calcium = dendritic_tree_get_total_calcium(tree);
    EXPECT_GE(total_calcium, 0.0f);
    EXPECT_LE(total_calcium, 100.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
