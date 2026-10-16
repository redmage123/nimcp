/**
 * @file test_claustrum_bio_async_integration.cpp
 * @brief Integration tests for Claustrum bio-async messaging
 *
 * WHAT: Tests bio-async messaging integration for consciousness binding
 * WHY:  Inter-module communication essential for brain-wide coordination
 * HOW:  Test message routing, handler registration, and async processing
 *
 * INTEGRATION POINTS:
 * - Bio-async router registration
 * - Message handler registration
 * - Cross-module binding notifications
 * - Workspace broadcast propagation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/claustrum/nimcp_claustrum.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ClaustrumBioAsyncTest : public ::testing::Test {
protected:
    nimcp_claustrum_t claustrum;
    nimcp_claustrum_config_t config;
    bool router_initialized;
    bio_module_context_t module_ctx;

    float test_features[8];

    void SetUp() override {
        router_initialized = false;
        module_ctx = NULL;
        memset(&claustrum, 0, sizeof(claustrum));

        /* Initialize bio-async router */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Initialize claustrum */
        config = nimcp_claustrum_default_config();
        config.enable_immune_reporting = true;
        config.enable_kg_integration = true;
        nimcp_claustrum_init(&claustrum, &config);

        /* Initialize test features */
        for (int i = 0; i < 8; i++) {
            test_features[i] = 0.5f + (float)i * 0.05f;
        }
    }

    void TearDown() override {
        if (module_ctx) {
            bio_router_unregister_module(module_ctx);
            module_ctx = NULL;
        }
        if (claustrum.initialized) {
            nimcp_claustrum_shutdown(&claustrum);
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }
};

/*=============================================================================
 * BIO-ROUTER INTEGRATION TESTS
 *===========================================================================*/

TEST_F(ClaustrumBioAsyncTest, RouterInitialized) {
    EXPECT_TRUE(router_initialized);
    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(ClaustrumBioAsyncTest, RegisterModuleWithRouter) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "claustrum_test";
    mod_info.inbox_capacity = 64;
    mod_info.user_data = &claustrum;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    const char* name = bio_module_context_get_name(module_ctx);
    EXPECT_NE(nullptr, name);
    EXPECT_STREQ("claustrum_test", name);
}

TEST_F(ClaustrumBioAsyncTest, GetModuleId) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "claustrum_id_test";
    mod_info.inbox_capacity = 64;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    bio_module_id_t id = bio_module_context_get_id(module_ctx);
    EXPECT_EQ(BIO_MODULE_BRAIN_REGION, id);
}

/*=============================================================================
 * HANDLER REGISTRATION TESTS
 *===========================================================================*/

static nimcp_error_t dummy_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)response_promise;
    (void)user_data;
    return NIMCP_OK;
}

TEST_F(ClaustrumBioAsyncTest, RegisterMessageHandler) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "handler_test";
    mod_info.inbox_capacity = 64;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    /* Register handler for workspace messages */
    nimcp_error_t err = bio_router_register_handler(
        module_ctx,
        (bio_message_type_t)0x0100,  /* Example message type */
        dummy_handler
    );
    EXPECT_EQ(NIMCP_OK, err);
}

TEST_F(ClaustrumBioAsyncTest, RegisterMultipleHandlers) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "multi_handler_test";
    mod_info.inbox_capacity = 64;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    /* Register multiple handlers */
    for (int i = 0; i < 5; i++) {
        nimcp_error_t err = bio_router_register_handler(
            module_ctx,
            (bio_message_type_t)(0x0100 + i),
            dummy_handler
        );
        EXPECT_EQ(NIMCP_OK, err) << "Handler " << i << " registration failed";
    }
}

TEST_F(ClaustrumBioAsyncTest, ClearHandlers) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "clear_handler_test";
    mod_info.inbox_capacity = 64;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    /* Register handler */
    bio_router_register_handler(module_ctx, (bio_message_type_t)0x0100, dummy_handler);

    /* Clear handlers */
    nimcp_error_t err = bio_router_clear_handlers(module_ctx);
    EXPECT_EQ(NIMCP_OK, err);
}

/*=============================================================================
 * MESSAGE PROCESSING TESTS
 *===========================================================================*/

TEST_F(ClaustrumBioAsyncTest, ProcessEmptyInbox) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "inbox_test";
    mod_info.inbox_capacity = 64;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    /* Process empty inbox - should return 0 */
    uint32_t processed = bio_router_process_inbox(module_ctx, 10);
    EXPECT_EQ(0u, processed);
}

TEST_F(ClaustrumBioAsyncTest, InboxCount) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "count_test";
    mod_info.inbox_capacity = 64;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    /* Empty inbox should have count 0 */
    uint32_t count = bio_router_inbox_count(module_ctx);
    EXPECT_EQ(0u, count);
}

/*=============================================================================
 * ROUTER STATISTICS TESTS
 *===========================================================================*/

TEST_F(ClaustrumBioAsyncTest, GetRouterStats) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_router_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    nimcp_error_t err = bio_router_get_stats(&stats);
    EXPECT_EQ(NIMCP_OK, err);

    /* Initial stats should be zeroed */
    EXPECT_EQ(0u, stats.messages_dropped);
    EXPECT_EQ(0u, stats.handler_errors);
}

TEST_F(ClaustrumBioAsyncTest, ResetRouterStats) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    /* Reset stats should not crash */
    bio_router_reset_stats();

    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    EXPECT_EQ(NIMCP_OK, err);
}

/*=============================================================================
 * INTEGRATION WITH CLAUSTRUM OPERATIONS
 *===========================================================================*/

TEST_F(ClaustrumBioAsyncTest, ClaustrumWithRouterActive) {
    /* Update claustrum with bio-async router active */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, test_features, 8, 0.8f);

    for (int i = 0; i < 10; i++) {
        nimcp_claustrum_update(&claustrum, 10.0f);

        /* Process router if initialized */
        if (router_initialized && module_ctx) {
            bio_router_process_inbox(module_ctx, 10);
        }
    }

    /* Claustrum should remain functional */
    EXPECT_TRUE(claustrum.initialized);
    EXPECT_EQ(CLAUSTRUM_STATUS_NORMAL, nimcp_claustrum_get_status(&claustrum));
}

TEST_F(ClaustrumBioAsyncTest, BindingWithRouterActive) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    /* Register module */
    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "binding_integration";
    mod_info.inbox_capacity = 64;
    mod_info.user_data = &claustrum;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    /* Full binding operation with bio-async router active */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, test_features, 8, 0.9f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, test_features, 8, 0.85f);

    for (int i = 0; i < 30; i++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
        bio_router_process_inbox(module_ctx, 10);
    }

    uint32_t modality_mask = (1 << CLAUSTRUM_MODALITY_VISUAL) | (1 << CLAUSTRUM_MODALITY_AUDITORY);
    uint32_t percept_id = 0;
    nimcp_claustrum_bind_modalities(&claustrum, modality_mask, &percept_id);

    /* Should not crash regardless of binding success */
    EXPECT_TRUE(claustrum.initialized);
}

TEST_F(ClaustrumBioAsyncTest, StateSwitchWithRouterActive) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    /* Register module */
    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "state_switch_test";
    mod_info.inbox_capacity = 64;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    nimcp_claustrum_switch_state(&claustrum, CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE);

    for (int i = 0; i < 50; i++) {
        nimcp_claustrum_update(&claustrum, 10.0f);
        bio_router_process_inbox(module_ctx, 10);
    }

    /* Verify state is functional */
    nimcp_claustrum_brain_state_t state = nimcp_claustrum_get_brain_state(&claustrum);
    EXPECT_TRUE(state == CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE ||
                state == CLAUSTRUM_BRAIN_STATE_TRANSITION);
}

/*=============================================================================
 * MESSAGE TYPE STRING TESTS
 *===========================================================================*/

TEST_F(ClaustrumBioAsyncTest, AllMessageTypeStringsValid) {
    for (int i = 0; i < CLAUSTRUM_BIO_MSG_COUNT; i++) {
        const char* str = nimcp_claustrum_bio_msg_type_string((nimcp_claustrum_bio_msg_type_t)i);
        EXPECT_NE(nullptr, str) << "Message type " << i << " has null string";
        EXPECT_GT(strlen(str), 0u) << "Message type " << i << " has empty string";
    }
}

TEST_F(ClaustrumBioAsyncTest, MessageTypeValues) {
    /* Verify message type enum values */
    EXPECT_EQ(0, CLAUSTRUM_BIO_MSG_BINDING);
    EXPECT_EQ(1, CLAUSTRUM_BIO_MSG_SYNC);
    EXPECT_EQ(2, CLAUSTRUM_BIO_MSG_SALIENCE);
    EXPECT_EQ(3, CLAUSTRUM_BIO_MSG_ATTENTION_BIAS);
    EXPECT_EQ(4, CLAUSTRUM_BIO_MSG_STATE_SWITCH);
    EXPECT_EQ(5, CLAUSTRUM_BIO_MSG_WORKSPACE_GATE);
    EXPECT_EQ(6, CLAUSTRUM_BIO_MSG_PERCEPT_BROADCAST);
    EXPECT_EQ(7, CLAUSTRUM_BIO_MSG_GAMMA_MODULATION);
    EXPECT_EQ(8, CLAUSTRUM_BIO_MSG_ALPHA_MODULATION);
    EXPECT_EQ(9, CLAUSTRUM_BIO_MSG_REQUEST_BINDING);
    EXPECT_EQ(10, CLAUSTRUM_BIO_MSG_MODALITY_UPDATE);
    EXPECT_EQ(11, CLAUSTRUM_BIO_MSG_CONSCIOUSNESS_CHANGE);
}

/*=============================================================================
 * ROUTER LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ClaustrumBioAsyncTest, RouterShutdownCleanup) {
    /* This test verifies that the fixture teardown works correctly */
    EXPECT_TRUE(router_initialized);

    /* Create and register a module */
    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "shutdown_test";
    mod_info.inbox_capacity = 32;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    /* Module context should be valid */
    EXPECT_NE(nullptr, bio_module_context_get_name(module_ctx));
}

TEST_F(ClaustrumBioAsyncTest, MultipleRouterCycles) {
    /* Test multiple init/shutdown cycles within a single test */
    for (int cycle = 0; cycle < 3; cycle++) {
        /* Shutdown existing router */
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }

        /* Reinitialize */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 32;
        router_config.inbox_capacity = 128;
        router_config.outbox_capacity = 128;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        EXPECT_TRUE(router_initialized) << "Cycle " << cycle << " failed";
        EXPECT_TRUE(bio_router_is_initialized()) << "Cycle " << cycle;
    }
}

/*=============================================================================
 * CONCURRENT MODULE TESTS
 *===========================================================================*/

TEST_F(ClaustrumBioAsyncTest, ConcurrentModuleProcessing) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    /* Create multiple modules */
    bio_module_context_t modules[3] = {NULL, NULL, NULL};

    for (int i = 0; i < 3; i++) {
        bio_module_info_t mod_info;
        memset(&mod_info, 0, sizeof(mod_info));
        mod_info.module_id = (bio_module_id_t)(BIO_MODULE_BRAIN_REGION + i + 100);
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "concurrent_%d", i);
        mod_info.module_name = name_buf;
        mod_info.inbox_capacity = 64;

        modules[i] = bio_router_register_module(&mod_info);
    }

    /* Process all */
    for (int cycle = 0; cycle < 20; cycle++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
        for (int i = 0; i < 3; i++) {
            if (modules[i]) {
                bio_router_process_inbox(modules[i], 10);
            }
        }
    }

    /* Cleanup */
    for (int i = 0; i < 3; i++) {
        if (modules[i]) {
            bio_router_unregister_module(modules[i]);
        }
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
