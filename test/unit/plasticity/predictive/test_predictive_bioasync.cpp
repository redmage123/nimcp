/**
 * @file test_predictive_bioasync.cpp
 * @brief Unit tests for predictive coding plasticity bio-async integration
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

class PredictiveBioAsyncTest : public ::testing::Test {
protected:
    pc_hierarchy_t hierarchy;
    uint32_t units_per_level[3];

    void SetUp() override {
        bio_router_config_t config;
        memset(&config, 0, sizeof(config));
        config.max_modules = 128;
        config.inbox_capacity = 64;
        bio_router_init(&config);

        // Create test hierarchy with persistent units array
        units_per_level[0] = 10;
        units_per_level[1] = 5;
        units_per_level[2] = 2;
        pc_hierarchy_config_t pc_config = pc_hierarchy_config_default(3, units_per_level);
        // Set the units_per_level pointer in the config
        pc_config.units_per_level = units_per_level;
        hierarchy = pc_hierarchy_create(&pc_config);
        ASSERT_NE(hierarchy, nullptr);
    }

    void TearDown() override {
        if (hierarchy) {
            pc_hierarchy_destroy(hierarchy);
        }
        bio_router_shutdown();
    }
};

TEST_F(PredictiveBioAsyncTest, ModuleRegistration) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_PREDICTIVE_CODING;
    bio_info.module_name = "predictive_coding";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_unregister_module(ctx);
}

TEST_F(PredictiveBioAsyncTest, InboxProcessing) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_PREDICTIVE_CODING;
    bio_info.module_name = "predictive_coding";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

    uint32_t processed = bio_router_process_inbox(ctx, 5);
    EXPECT_EQ(processed, 0u);

    bio_router_unregister_module(ctx);
}

TEST_F(PredictiveBioAsyncTest, BroadcastMessage) {
    bio_module_info_t bio_info;
    memset(&bio_info, 0, sizeof(bio_info));
    bio_info.module_id = BIO_MODULE_PREDICTIVE_CODING;
    bio_info.module_name = "predictive_coding";
    bio_info.inbox_capacity = 64;
    bio_info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&bio_info);
    ASSERT_NE(ctx, nullptr);

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

TEST_F(PredictiveBioAsyncTest, FreeEnergyComputation) {
    // Run one inference step
    float input[] = {0.5f, 0.3f, 0.8f, 0.1f, 0.6f, 0.4f, 0.9f, 0.2f, 0.7f, 0.0f};
    pc_hierarchy_set_input(hierarchy, input);
    pc_hierarchy_inference_step(hierarchy, 1.0f, false);

    float free_energy = pc_hierarchy_get_free_energy(hierarchy);
    // Allow small negative values due to numerical precision
    EXPECT_GE(free_energy, -0.01f);
    EXPECT_LT(free_energy, 1000.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
