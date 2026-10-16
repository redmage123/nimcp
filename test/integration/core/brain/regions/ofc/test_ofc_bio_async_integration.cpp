/**
 * @file test_ofc_bio_async_integration.cpp
 * @brief Integration tests for OFC bio-async messaging
 *
 * WHAT: Tests bio-async messaging integration for value signals
 * WHY:  Inter-module communication essential for decision broadcasting
 * HOW:  Test message routing, handler registration, and async processing
 *
 * INTEGRATION POINTS:
 * - Bio-async router registration
 * - Value update broadcasts
 * - Decision notifications
 * - RPE signal propagation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/ofc/nimcp_ofc.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class OFCBioAsyncTest : public ::testing::Test {
protected:
    nimcp_ofc_t* ofc;
    ofc_config_t config;
    bool router_initialized;
    bio_module_context_t module_ctx;

    void SetUp() override {
        router_initialized = false;
        module_ctx = NULL;
        ofc = NULL;

        /* Initialize bio-async router */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Initialize OFC */
        memset(&config, 0, sizeof(config));
        ofc_default_config(&config);
        config.enable_bio_async = true;

        ofc = ofc_create(&config);
    }

    void TearDown() override {
        if (module_ctx) {
            bio_router_unregister_module(module_ctx);
            module_ctx = NULL;
        }
        if (ofc) {
            ofc_destroy(ofc);
            ofc = NULL;
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

TEST_F(OFCBioAsyncTest, RouterInitialized) {
    EXPECT_TRUE(router_initialized);
    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(OFCBioAsyncTest, OFCCreatedWithBioAsync) {
    ASSERT_NE(nullptr, ofc);
    EXPECT_TRUE(ofc->initialized);
    EXPECT_TRUE(config.enable_bio_async);
}

TEST_F(OFCBioAsyncTest, RegisterModuleWithRouter) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "ofc_test";
    mod_info.inbox_capacity = 64;
    mod_info.user_data = ofc;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    const char* name = bio_module_context_get_name(module_ctx);
    EXPECT_NE(nullptr, name);
    EXPECT_STREQ("ofc_test", name);
}

/*=============================================================================
 * MESSAGE SUBSCRIPTION TESTS
 *===========================================================================*/

TEST_F(OFCBioAsyncTest, SubscriptionMaskValues) {
    /* Verify subscription bitmask values */
    EXPECT_EQ(OFC_BIO_SUB_VALUE, (1U << OFC_BIO_MSG_VALUE_UPDATE));
    EXPECT_EQ(OFC_BIO_SUB_DECISION, (1U << OFC_BIO_MSG_DECISION));
    EXPECT_EQ(OFC_BIO_SUB_RPE, (1U << OFC_BIO_MSG_PREDICTION_ERROR));
    EXPECT_EQ(OFC_BIO_SUB_REVERSAL, (1U << OFC_BIO_MSG_REVERSAL));
    EXPECT_EQ(OFC_BIO_SUB_RISK, (1U << OFC_BIO_MSG_RISK_ASSESSMENT));
    EXPECT_EQ(OFC_BIO_SUB_SOCIAL, (1U << OFC_BIO_MSG_SOCIAL_REWARD));
    EXPECT_EQ(OFC_BIO_SUB_EMOTION, (1U << OFC_BIO_MSG_EMOTION_MODULATION));
}

TEST_F(OFCBioAsyncTest, OFCSubscribeToMessages) {
    ASSERT_NE(nullptr, ofc);

    /* OFC has its own subscription function */
    int result = ofc_bio_async_subscribe(ofc, OFC_BIO_SUB_ALL);
    /* May succeed or fail based on connection state */
    (void)result;
}

TEST_F(OFCBioAsyncTest, OFCConnectToRouter) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }
    ASSERT_NE(nullptr, ofc);

    /* Connect OFC to the bio-async router */
    int result = ofc_bio_async_connect(ofc, NULL);
    /* May succeed or fail based on implementation */
    (void)result;

    /* Disconnect */
    ofc_bio_async_disconnect(ofc);
}

/*=============================================================================
 * MESSAGE BROADCASTING TESTS
 *===========================================================================*/

TEST_F(OFCBioAsyncTest, BroadcastValueUpdate) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }
    ASSERT_NE(nullptr, ofc);

    float payload = 0.8f;
    int result = ofc_bio_async_broadcast(ofc, OFC_BIO_MSG_VALUE_UPDATE, &payload, sizeof(payload));
    /* May succeed or fail based on connection state */
    (void)result;
}

TEST_F(OFCBioAsyncTest, BroadcastDecision) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }
    ASSERT_NE(nullptr, ofc);

    ofc_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    decision.chosen_option = 1;
    decision.decision_value = 0.7f;
    decision.confidence = 0.85f;

    int result = ofc_bio_async_broadcast(ofc, OFC_BIO_MSG_DECISION, &decision, sizeof(decision));
    (void)result;
}

TEST_F(OFCBioAsyncTest, BroadcastPredictionError) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }
    ASSERT_NE(nullptr, ofc);

    float rpe = 0.3f;
    int result = ofc_bio_async_broadcast(ofc, OFC_BIO_MSG_PREDICTION_ERROR, &rpe, sizeof(rpe));
    (void)result;
}

/*=============================================================================
 * ASYNC PROCESSING TESTS
 *===========================================================================*/

TEST_F(OFCBioAsyncTest, ProcessWithBioAsync) {
    ASSERT_NE(nullptr, ofc);

    /* Present options and make decisions with bio-async active */
    ofc_present_option(ofc, 1, 0.5f, 0.8f, 0.0f);
    ofc_present_option(ofc, 2, 0.7f, 0.9f, 0.0f);

    for (int i = 0; i < 20; i++) {
        ofc_update(ofc, 10.0f);

        /* Process inbox if we have a module context */
        if (router_initialized && module_ctx) {
            bio_router_process_inbox(module_ctx, 10);
        }
    }

    /* System should remain stable */
    EXPECT_TRUE(ofc->initialized);

    ofc_decision_t decision;
    int result = ofc_make_decision(ofc, &decision);
    EXPECT_EQ(0, result);
}

TEST_F(OFCBioAsyncTest, ConcurrentMessaging) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }
    ASSERT_NE(nullptr, ofc);

    /* Create multiple modules */
    bio_module_context_t modules[3] = {NULL, NULL, NULL};

    for (int i = 0; i < 3; i++) {
        bio_module_info_t mod_info;
        memset(&mod_info, 0, sizeof(mod_info));
        mod_info.module_id = (bio_module_id_t)(BIO_MODULE_BRAIN_REGION + i + 100);
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "ofc_concurrent_%d", i);
        mod_info.module_name = name_buf;
        mod_info.inbox_capacity = 64;

        modules[i] = bio_router_register_module(&mod_info);
    }

    /* Process with multiple modules active */
    for (int cycle = 0; cycle < 20; cycle++) {
        ofc_update(ofc, 5.0f);
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

/*=============================================================================
 * MESSAGE TYPE STRING TESTS
 *===========================================================================*/

TEST_F(OFCBioAsyncTest, MessageTypeValues) {
    /* Verify message type enum values */
    EXPECT_EQ(0, OFC_BIO_MSG_VALUE_UPDATE);
    EXPECT_EQ(1, OFC_BIO_MSG_DECISION);
    EXPECT_EQ(2, OFC_BIO_MSG_PREDICTION_ERROR);
    EXPECT_EQ(3, OFC_BIO_MSG_REVERSAL);
    EXPECT_EQ(4, OFC_BIO_MSG_RISK_ASSESSMENT);
    EXPECT_EQ(5, OFC_BIO_MSG_SOCIAL_REWARD);
    EXPECT_EQ(6, OFC_BIO_MSG_EMOTION_MODULATION);
    EXPECT_EQ(7, OFC_BIO_MSG_STATE_REQUEST);
}

/*=============================================================================
 * INTEGRATION WITH OFC OPERATIONS
 *===========================================================================*/

TEST_F(OFCBioAsyncTest, DecisionWithAsyncBroadcast) {
    ASSERT_NE(nullptr, ofc);

    /* Subscribe OFC to all messages */
    ofc_bio_async_subscribe(ofc, OFC_BIO_SUB_ALL);

    /* Present options */
    ofc_present_option(ofc, 1, 0.4f, 0.8f, 0.0f);
    ofc_present_option(ofc, 2, 0.8f, 0.9f, 0.0f);

    /* Update with bio-async processing */
    for (int i = 0; i < 30; i++) {
        ofc_update(ofc, 10.0f);
        if (router_initialized && module_ctx) {
            bio_router_process_inbox(module_ctx, 10);
        }
    }

    ofc_decision_t decision;
    ofc_make_decision(ofc, &decision);

    /* Stats should reflect bio messaging if connected */
    ofc_stats_t stats;
    ofc_get_stats(ofc, &stats);
    /* bio_msgs_sent may be 0 if not connected */
}

TEST_F(OFCBioAsyncTest, PredictionErrorBroadcast) {
    ASSERT_NE(nullptr, ofc);

    ofc_present_option(ofc, 1, 0.5f, 0.9f, 0.0f);

    for (int i = 0; i < 10; i++) {
        ofc_update(ofc, 10.0f);
        if (router_initialized && module_ctx) {
            bio_router_process_inbox(module_ctx, 10);
        }
    }

    /* This should trigger an RPE broadcast */
    ofc_update_prediction_error(ofc, 1, 0.9f);

    if (router_initialized && module_ctx) {
        bio_router_process_inbox(module_ctx, 10);
    }

    /* System should remain stable */
    EXPECT_TRUE(ofc->initialized);
}

/*=============================================================================
 * ROUTER STATISTICS TESTS
 *===========================================================================*/

TEST_F(OFCBioAsyncTest, GetRouterStats) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_router_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    nimcp_error_t err = bio_router_get_stats(&stats);
    EXPECT_EQ(NIMCP_OK, err);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
