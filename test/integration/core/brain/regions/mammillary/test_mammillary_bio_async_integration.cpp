/**
 * @file test_mammillary_bio_async_integration.cpp
 * @brief Integration tests for Mammillary Bodies with Bio-Async system
 *
 * WHAT: Tests Mammillary Bodies integration with bio-async messaging
 * WHY:  Ensure proper async communication and background consolidation
 * HOW:  Test bio-async bridge initialization and async operations
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "nimcp.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/mammillary/nimcp_mammillary.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MammillaryBioAsyncIntegrationTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mammillary;
    mammillary_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;

        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        config = mammillary_default_config();
        config.enable_background_consolidation = true;
        config.enable_papez_circuit = true;
        mammillary = mammillary_create(&config);
        ASSERT_NE(nullptr, mammillary);
    }

    void TearDown() override {
        if (mammillary) {
            mammillary_destroy(mammillary);
            mammillary = nullptr;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    void CreateTestContext(float* context, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            context[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * BIO-ASYNC BRIDGE TESTS
 *===========================================================================*/

TEST_F(MammillaryBioAsyncIntegrationTest, RouterInitialized) {
    EXPECT_TRUE(router_initialized);
}

TEST_F(MammillaryBioAsyncIntegrationTest, InitBioAsyncBridge) {
    if (!router_initialized) {
        GTEST_SKIP() << "Router not initialized";
    }
    int result = mammillary_init_bio_async_bridge(mammillary, nullptr);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBioAsyncIntegrationTest, BridgeInitializedFlag) {
    if (!router_initialized) {
        GTEST_SKIP() << "Router not initialized";
    }
    mammillary_init_bio_async_bridge(mammillary, nullptr);

    /* Bridge should be marked as initialized */
    mammillary_status_t status = mammillary_get_status(mammillary);
    EXPECT_NE(status, MAMMILLARY_STATUS_ERROR);
}

TEST_F(MammillaryBioAsyncIntegrationTest, UpdateWithBioAsync) {
    if (!router_initialized) {
        GTEST_SKIP() << "Router not initialized";
    }
    mammillary_init_bio_async_bridge(mammillary, nullptr);

    int result = mammillary_update(mammillary, 10.0f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * ASYNC OPERATION TESTS
 *===========================================================================*/

TEST_F(MammillaryBioAsyncIntegrationTest, ProcessIncoming) {
    if (!router_initialized) {
        GTEST_SKIP() << "Router not initialized";
    }
    mammillary_init_bio_async_bridge(mammillary, nullptr);

    int result = mammillary_process_incoming(mammillary);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBioAsyncIntegrationTest, SendOutgoing) {
    if (!router_initialized) {
        GTEST_SKIP() << "Router not initialized";
    }
    mammillary_init_bio_async_bridge(mammillary, nullptr);

    int result = mammillary_send_outgoing(mammillary);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBioAsyncIntegrationTest, BidirectionalWithAsync) {
    if (!router_initialized) {
        GTEST_SKIP() << "Router not initialized";
    }
    mammillary_init_bio_async_bridge(mammillary, nullptr);

    for (int i = 0; i < 10; i++) {
        int result = mammillary_bidirectional_update(mammillary, 10.0f);
        EXPECT_EQ(0, result);
    }
}

/*=============================================================================
 * BACKGROUND CONSOLIDATION TESTS
 *===========================================================================*/

TEST_F(MammillaryBioAsyncIntegrationTest, BackgroundConsolidationEnabled) {
    mammillary_config_t retrieved;
    mammillary_get_config(mammillary, &retrieved);
    EXPECT_TRUE(retrieved.enable_background_consolidation);
}

TEST_F(MammillaryBioAsyncIntegrationTest, ConsolidationWithAsync) {
    if (!router_initialized) {
        GTEST_SKIP() << "Router not initialized";
    }
    mammillary_init_bio_async_bridge(mammillary, nullptr);

    /* Encode a memory trace */
    float position[3] = {1.0f, 2.0f, 0.0f};
    float context[128];
    CreateTestContext(context, 128, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_encode_spatial_memory(mammillary, position, 0.0f,
        context, 128, &trace_id));

    /* Run updates with async bridge */
    for (int i = 0; i < 50; i++) {
        mammillary_update(mammillary, 10.0f);
    }

    mammillary_stats_t stats;
    mammillary_get_stats(mammillary, &stats);
    EXPECT_GE(stats.total_memory_traces, 1u);
}

/*=============================================================================
 * ROUTER OPERATION TESTS
 *===========================================================================*/

TEST_F(MammillaryBioAsyncIntegrationTest, MultipleRouterOperations) {
    if (!router_initialized) {
        GTEST_SKIP() << "Router not initialized";
    }
    mammillary_init_bio_async_bridge(mammillary, nullptr);

    for (int i = 0; i < 5; i++) {
        mammillary_process_incoming(mammillary);
        mammillary_update(mammillary, 10.0f);
        mammillary_send_outgoing(mammillary);
    }

    mammillary_stats_t stats;
    mammillary_get_stats(mammillary, &stats);
    EXPECT_GE(stats.updates_processed, 5u);
}

/*=============================================================================
 * ASYNC ERROR RECOVERY TESTS
 *===========================================================================*/

TEST_F(MammillaryBioAsyncIntegrationTest, OperationsWithoutBridge) {
    /* Operations should still work without bio-async bridge */
    int result = mammillary_update(mammillary, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBioAsyncIntegrationTest, ResetWithBioAsync) {
    if (!router_initialized) {
        GTEST_SKIP() << "Router not initialized";
    }
    mammillary_init_bio_async_bridge(mammillary, nullptr);

    mammillary_reset(mammillary);

    /* Should still function after reset */
    int result = mammillary_update(mammillary, 10.0f);
    EXPECT_EQ(0, result);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
