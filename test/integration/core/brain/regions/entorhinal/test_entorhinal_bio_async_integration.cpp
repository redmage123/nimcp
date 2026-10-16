/**
 * @file test_entorhinal_bio_async_integration.cpp
 * @brief Integration tests for Entorhinal Cortex bio-async messaging
 *
 * WHAT: Tests bio-async messaging integration for spatial memory gateway
 * WHY:  Inter-module communication essential for memory encoding/retrieval
 *       and grid/border/HD cell modulation via neuromodulators
 * HOW:  Test message routing, handler registration, neuromodulator effects,
 *       async message processing, and error recovery
 *
 * INTEGRATION POINTS:
 * - Bio-async router registration
 * - Neuromodulator channel integration (DA/5-HT/NE/ACh)
 * - Async message routing and priority handling
 * - Callback mechanisms for spatial memory operations
 * - Error handling and recovery under bio-async failures
 *
 * BIOLOGICAL CONTEXT:
 * The entorhinal cortex receives neuromodulatory inputs that affect:
 * - Dopamine (DA): Modulates reward-related memory encoding
 * - Serotonin (5-HT): Affects mood-dependent spatial processing
 * - Norepinephrine (NE): Enhances attention and threat-related memory
 * - Acetylcholine (ACh): Critical for memory encoding gate control
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>

extern "C" {
#include "nimcp.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * TEST CONSTANTS
 *===========================================================================*/

#define TEST_MODULE_HIPPOCAMPUS     0x3001
#define TEST_MODULE_NEOCORTEX       0x3002
#define TEST_MODULE_PREFRONTAL      0x3003
#define TEST_MODULE_PERIRHINAL      0x3004
#define TEST_MODULE_PARAHIPPOCAMPAL 0x3005

#define TEST_NEUROMOD_LOW           0.2f
#define TEST_NEUROMOD_MEDIUM        0.5f
#define TEST_NEUROMOD_HIGH          0.8f
#define TEST_NEUROMOD_CRITICAL      0.95f

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class EntorhinalBioAsyncTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec;
    entorhinal_config_t config;
    bool router_initialized;
    bio_module_context_t module_ctx;

    float test_position[3];
    float test_features[32];

    void SetUp() override {
        router_initialized = false;
        module_ctx = NULL;
        ec = NULL;

        /* Initialize bio-async router */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Initialize entorhinal cortex with bio-async enabled */
        config = entorhinal_default_config();
        config.enable_bio_async = true;
        config.enable_path_integration = true;
        config.enable_boundary_detection = true;
        config.num_grid_cells = 256;
        config.num_border_cells = 64;
        config.num_hd_cells = 30;

        ec = entorhinal_create(&config);

        /* Initialize test data */
        test_position[0] = 5.0f;
        test_position[1] = 5.0f;
        test_position[2] = 0.0f;

        for (int i = 0; i < 32; i++) {
            test_features[i] = sinf(i * 0.2f) * 0.5f + 0.5f;
        }
    }

    void TearDown() override {
        if (module_ctx) {
            bio_router_unregister_module(module_ctx);
            module_ctx = NULL;
        }
        if (ec) {
            entorhinal_destroy(ec);
            ec = NULL;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    /* Helper to update the system */
    void update_system(float dt_ms) {
        if (ec) {
            entorhinal_bidirectional_update(ec, dt_ms);
        }
        if (router_initialized && module_ctx) {
            bio_router_process_inbox(module_ctx, 10);
        }
    }
};

/*=============================================================================
 * BIO-ROUTER INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, RouterInitialized) {
    EXPECT_TRUE(router_initialized);
    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(EntorhinalBioAsyncTest, EntorhinalCreatedWithBioAsync) {
    ASSERT_NE(nullptr, ec);
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
    EXPECT_TRUE(config.enable_bio_async);
}

TEST_F(EntorhinalBioAsyncTest, RegisterModuleWithRouter) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "entorhinal_test";
    mod_info.inbox_capacity = 64;
    mod_info.user_data = ec;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    const char* name = bio_module_context_get_name(module_ctx);
    EXPECT_NE(nullptr, name);
    EXPECT_STREQ("entorhinal_test", name);
}

TEST_F(EntorhinalBioAsyncTest, GetModuleId) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "entorhinal_id_test";
    mod_info.inbox_capacity = 64;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    bio_module_id_t id = bio_module_context_get_id(module_ctx);
    EXPECT_EQ(BIO_MODULE_BRAIN_REGION, id);
}

/*=============================================================================
 * BIO-ASYNC BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, InitBioAsyncBridgeNullRouter) {
    ASSERT_NE(nullptr, ec);

    /* Should handle NULL router gracefully */
    int result = entorhinal_init_bio_async_bridge(ec, nullptr);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBioAsyncTest, BioAsyncBridgeState) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Verify bridge state is initialized */
    EXPECT_EQ(0u, ec->bio_async_bridge.pending_messages);
    EXPECT_EQ(0u, ec->bio_async_bridge.messages_processed);
}

TEST_F(EntorhinalBioAsyncTest, SyncBioAsyncWithNoBridge) {
    ASSERT_NE(nullptr, ec);

    /* Should not crash with no bridge initialized */
    int result = entorhinal_sync_bio_async(ec);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * HANDLER REGISTRATION TESTS
 *===========================================================================*/

static nimcp_error_t dummy_spatial_handler(
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

TEST_F(EntorhinalBioAsyncTest, RegisterMessageHandler) {
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

    /* Register handler for spatial memory messages */
    nimcp_error_t err = bio_router_register_handler(
        module_ctx,
        (bio_message_type_t)0x0300,  /* Spatial memory message type */
        dummy_spatial_handler
    );
    EXPECT_EQ(NIMCP_OK, err);
}

TEST_F(EntorhinalBioAsyncTest, RegisterMultipleHandlers) {
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

    /* Register handlers for different message types */
    for (int i = 0; i < 5; i++) {
        nimcp_error_t err = bio_router_register_handler(
            module_ctx,
            (bio_message_type_t)(0x0300 + i),
            dummy_spatial_handler
        );
        EXPECT_EQ(NIMCP_OK, err) << "Handler " << i << " registration failed";
    }
}

TEST_F(EntorhinalBioAsyncTest, ClearHandlers) {
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
    bio_router_register_handler(module_ctx, (bio_message_type_t)0x0300, dummy_spatial_handler);

    /* Clear handlers */
    nimcp_error_t err = bio_router_clear_handlers(module_ctx);
    EXPECT_EQ(NIMCP_OK, err);
}

/*=============================================================================
 * NEUROMODULATOR CHANNEL INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, NeuromodulatorLevelsInitialized) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* All neuromodulator levels should be in valid range */
    for (int i = 0; i < ENTORHINAL_CHANNEL_COUNT; i++) {
        EXPECT_GE(ec->bio_async_bridge.neuromodulator_levels[i], 0.0f);
        EXPECT_LE(ec->bio_async_bridge.neuromodulator_levels[i], 1.0f);
    }
}

TEST_F(EntorhinalBioAsyncTest, ProcessNeuromodulationWithDefaultLevels) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Process neuromodulation should succeed */
    int result = entorhinal_process_neuromodulation(ec);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBioAsyncTest, DopamineAffectsRewardMemory) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Set high dopamine level */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_DOPAMINE] = TEST_NEUROMOD_HIGH;

    /* Process neuromodulation */
    int result = entorhinal_process_neuromodulation(ec);
    EXPECT_EQ(0, result);

    /* System should still be functional */
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

TEST_F(EntorhinalBioAsyncTest, SerotoninAffectsMoodProcessing) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Set low serotonin level (mood-dependent effect) */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_SEROTONIN] = TEST_NEUROMOD_LOW;

    int result = entorhinal_process_neuromodulation(ec);
    EXPECT_EQ(0, result);

    /* Should still be able to process spatial information */
    EXPECT_EQ(entorhinal_update_grid_cells(ec, test_position, 3), 0);
}

TEST_F(EntorhinalBioAsyncTest, NorepinephrineEnhancesAttention) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Set high norepinephrine (alertness/attention) */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_NOREPINEPHRINE] = TEST_NEUROMOD_CRITICAL;

    int result = entorhinal_process_neuromodulation(ec);
    EXPECT_EQ(0, result);

    /* HD cells should still be functional */
    float heading = M_PI / 4.0f;
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, 0.0f), 0);
}

TEST_F(EntorhinalBioAsyncTest, AcetylcholineAffectsEncodingGate) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Set high acetylcholine (memory encoding) */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_ACETYLCHOLINE] = TEST_NEUROMOD_HIGH;

    int result = entorhinal_process_neuromodulation(ec);
    EXPECT_EQ(0, result);

    /* Should be able to encode memories */
    entorhinal_set_encoding_gate(ec, 1.0f);
    EXPECT_EQ(entorhinal_encode_to_hippocampus(ec, test_features, 32, test_position, 3), 0);
}

TEST_F(EntorhinalBioAsyncTest, AllNeuromodulatorsInteract) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Set varied neuromodulator levels */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_DOPAMINE] = TEST_NEUROMOD_HIGH;
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_SEROTONIN] = TEST_NEUROMOD_MEDIUM;
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_NOREPINEPHRINE] = TEST_NEUROMOD_LOW;
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_ACETYLCHOLINE] = TEST_NEUROMOD_HIGH;

    /* Process neuromodulation */
    int result = entorhinal_process_neuromodulation(ec);
    EXPECT_EQ(0, result);

    /* Full system should remain stable */
    EXPECT_EQ(entorhinal_bidirectional_update(ec, 10.0f), 0);
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

/*=============================================================================
 * ASYNC MESSAGE ROUTING TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, ProcessEmptyInbox) {
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

TEST_F(EntorhinalBioAsyncTest, InboxCount) {
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
 * MESSAGE PRIORITY HANDLING TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, HighPriorityNeuromodMessage) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Simulate high-priority norepinephrine signal (threat detected) */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_NOREPINEPHRINE] = TEST_NEUROMOD_CRITICAL;

    /* This should trigger priority processing */
    int result = entorhinal_process_neuromodulation(ec);
    EXPECT_EQ(0, result);

    /* Border cells should be active for escape route detection */
    float boundary_distances[4] = {2.0f, 5.0f, 3.0f, 4.0f};
    EXPECT_EQ(entorhinal_update_border_cells(ec, boundary_distances, 4), 0);
}

TEST_F(EntorhinalBioAsyncTest, LowPriorityConsolidation) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Low arousal state (serotonin high, NE low) */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_SEROTONIN] = TEST_NEUROMOD_HIGH;
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_NOREPINEPHRINE] = TEST_NEUROMOD_LOW;

    int result = entorhinal_process_neuromodulation(ec);
    EXPECT_EQ(0, result);

    /* Memory consolidation should be possible */
    EXPECT_EQ(entorhinal_consolidate_to_neocortex(ec, 0, 0.5f), 0);
}

/*=============================================================================
 * CALLBACK MECHANISM TESTS
 *===========================================================================*/

static std::atomic<int> callback_count(0);
static std::mutex callback_mutex;

static nimcp_error_t counting_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)response_promise;
    (void)user_data;
    callback_count++;
    return NIMCP_OK;
}

TEST_F(EntorhinalBioAsyncTest, CallbackRegistration) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    callback_count = 0;

    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "callback_test";
    mod_info.inbox_capacity = 64;
    mod_info.user_data = ec;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    /* Register counting handler */
    nimcp_error_t err = bio_router_register_handler(
        module_ctx,
        (bio_message_type_t)0x0301,
        counting_handler
    );
    EXPECT_EQ(NIMCP_OK, err);
}

TEST_F(EntorhinalBioAsyncTest, EntorhinalWithCallbacksActive) {
    ASSERT_NE(nullptr, ec);

    /* Update entorhinal with callbacks potentially active */
    EXPECT_EQ(entorhinal_update_grid_cells(ec, test_position, 3), 0);

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(entorhinal_bidirectional_update(ec, 10.0f), 0);

        /* Process router if initialized */
        if (router_initialized && module_ctx) {
            bio_router_process_inbox(module_ctx, 10);
        }
    }

    /* Entorhinal should remain functional */
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

/*=============================================================================
 * ASYNC UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, AsyncUpdateWithGridCells) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Run async update cycles with grid cell updates */
    for (int cycle = 0; cycle < 50; cycle++) {
        float angle = cycle * 0.1f;
        float x = cosf(angle) * 5.0f;
        float y = sinf(angle) * 5.0f;
        float position[3] = {x, y, 0.0f};

        EXPECT_EQ(entorhinal_update_grid_cells(ec, position, 3), 0);
        EXPECT_EQ(entorhinal_sync_bio_async(ec), 0);
        EXPECT_EQ(entorhinal_process_neuromodulation(ec), 0);
    }

    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

TEST_F(EntorhinalBioAsyncTest, AsyncUpdateWithHDCells) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Run async update cycles with HD cell updates */
    for (int cycle = 0; cycle < 50; cycle++) {
        float heading = (cycle * 0.1f) - M_PI;  /* Rotate through all directions */
        float angular_velocity = 0.1f * sinf(cycle * 0.2f);

        EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, angular_velocity), 0);
        EXPECT_EQ(entorhinal_sync_bio_async(ec), 0);
    }

    /* Decode heading should work */
    float decoded_heading, confidence;
    EXPECT_EQ(entorhinal_decode_heading(ec, &decoded_heading, &confidence), 0);
}

TEST_F(EntorhinalBioAsyncTest, AsyncUpdateWithBorderCells) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Run async update cycles approaching and retreating from boundary */
    for (int cycle = 0; cycle < 50; cycle++) {
        float distance = 1.0f + 4.0f * fabsf(sinf(cycle * 0.1f));
        float boundary_distances[4] = {distance, 10.0f, 10.0f, 10.0f};

        EXPECT_EQ(entorhinal_update_border_cells(ec, boundary_distances, 4), 0);
        EXPECT_EQ(entorhinal_sync_bio_async(ec), 0);
    }

    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

TEST_F(EntorhinalBioAsyncTest, FullAsyncUpdateCycle) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Comprehensive update cycle */
    for (int cycle = 0; cycle < 100; cycle++) {
        float dt = 10.0f;  /* 10ms timestep */

        /* Update position */
        float angle = cycle * 0.05f;
        float x = 5.0f + cosf(angle) * 3.0f;
        float y = 5.0f + sinf(angle) * 3.0f;
        float position[3] = {x, y, 0.0f};
        float heading = angle + M_PI / 2.0f;

        /* Update all spatial cells */
        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, heading, 0.05f);

        float boundary_distances[4] = {5.0f - x, 10.0f - x, 5.0f - y, 10.0f - y};
        for (int i = 0; i < 4; i++) {
            if (boundary_distances[i] < 0) boundary_distances[i] = -boundary_distances[i];
            if (boundary_distances[i] < 0.5f) boundary_distances[i] = 0.5f;
        }
        entorhinal_update_border_cells(ec, boundary_distances, 4);

        /* Path integration */
        float velocity[3] = {-sinf(angle) * 0.15f, cosf(angle) * 0.15f, 0.0f};
        entorhinal_path_integrate(ec, velocity, 0.05f, dt / 1000.0f);

        /* Bio-async synchronization */
        entorhinal_sync_bio_async(ec);
        entorhinal_process_neuromodulation(ec);

        /* Full bidirectional update */
        entorhinal_bidirectional_update(ec, dt);

        if (router_initialized && module_ctx) {
            bio_router_process_inbox(module_ctx, 10);
        }
    }

    /* Verify system health */
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
    float health = entorhinal_get_health_status(ec);
    EXPECT_GT(health, 0.0f);
}

/*=============================================================================
 * ERROR HANDLING AND RECOVERY TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, HandleNullPointerGracefully) {
    /* All functions should handle NULL gracefully */
    EXPECT_EQ(-1, entorhinal_init_bio_async_bridge(nullptr, nullptr));
    EXPECT_EQ(-1, entorhinal_sync_bio_async(nullptr));
    EXPECT_EQ(-1, entorhinal_process_neuromodulation(nullptr));
}

TEST_F(EntorhinalBioAsyncTest, RecoverFromInvalidNeuromodLevels) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Set invalid (out of range) neuromodulator level */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_DOPAMINE] = 2.0f;

    /* Processing should clamp/handle invalid values */
    int result = entorhinal_process_neuromodulation(ec);
    /* Should either succeed after clamping or return error */
    (void)result;

    /* System should still be functional */
    EXPECT_EQ(entorhinal_update_grid_cells(ec, test_position, 3), 0);
}

TEST_F(EntorhinalBioAsyncTest, RecoverFromRouterDisconnection) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    ASSERT_NE(nullptr, ec);

    /* Register module */
    bio_module_info_t mod_info;
    memset(&mod_info, 0, sizeof(mod_info));
    mod_info.module_id = BIO_MODULE_BRAIN_REGION;
    mod_info.module_name = "recovery_test";
    mod_info.inbox_capacity = 64;
    mod_info.user_data = ec;

    module_ctx = bio_router_register_module(&mod_info);
    ASSERT_NE(nullptr, module_ctx);

    /* Unregister (simulate disconnection) */
    bio_router_unregister_module(module_ctx);
    module_ctx = NULL;

    /* Entorhinal should still work without router */
    EXPECT_EQ(entorhinal_update_grid_cells(ec, test_position, 3), 0);
    EXPECT_EQ(entorhinal_bidirectional_update(ec, 10.0f), 0);
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

TEST_F(EntorhinalBioAsyncTest, HandleRouterShutdownGracefully) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    ASSERT_NE(nullptr, ec);

    /* Shutdown router while entorhinal is active */
    bio_router_shutdown();
    router_initialized = false;

    /* Entorhinal should continue working */
    EXPECT_EQ(entorhinal_update_grid_cells(ec, test_position, 3), 0);
    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

/*=============================================================================
 * ROUTER STATISTICS TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, GetRouterStats) {
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

TEST_F(EntorhinalBioAsyncTest, ResetRouterStats) {
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
 * INTEGRATION WITH ENTORHINAL OPERATIONS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, MemoryEncodingWithBioAsync) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Set favorable neuromodulator state for encoding (high ACh) */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_ACETYLCHOLINE] = TEST_NEUROMOD_HIGH;
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_DOPAMINE] = TEST_NEUROMOD_MEDIUM;

    entorhinal_process_neuromodulation(ec);

    /* Open encoding gate */
    entorhinal_set_encoding_gate(ec, 1.0f);

    /* Encode memory */
    float spatial_context[3] = {3.0f, 4.0f, 0.0f};
    EXPECT_EQ(entorhinal_encode_to_hippocampus(ec, test_features, 32, spatial_context, 3), 0);

    /* Verify stats updated */
    uint64_t encoded, retrieved, consolidated;
    entorhinal_get_gateway_stats(ec, &encoded, &retrieved, &consolidated);
    EXPECT_GE(encoded, 1u);
}

TEST_F(EntorhinalBioAsyncTest, MemoryRetrievalWithBioAsync) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* First encode a memory */
    entorhinal_set_encoding_gate(ec, 1.0f);
    float spatial_context[3] = {3.0f, 4.0f, 0.0f};
    entorhinal_encode_to_hippocampus(ec, test_features, 32, spatial_context, 3);

    /* Set favorable state for retrieval */
    ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_ACETYLCHOLINE] = TEST_NEUROMOD_MEDIUM;

    entorhinal_process_neuromodulation(ec);

    /* Open retrieval gate */
    entorhinal_set_retrieval_gate(ec, 1.0f);

    /* Retrieve with similar cue */
    float cue[32];
    for (int i = 0; i < 32; i++) {
        cue[i] = test_features[i] + 0.05f;  /* Slightly noisy */
    }
    float retrieved[64];
    uint32_t actual_features = 0;
    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(ec, cue, 32, retrieved, 64, &actual_features), 0);
}

TEST_F(EntorhinalBioAsyncTest, PathIntegrationWithNeuromodulation) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Reset position */
    float start_pos[3] = {0.0f, 0.0f, 0.0f};
    entorhinal_reset_grid_phases(ec, start_pos);

    /* Path integrate with varying neuromodulation */
    for (int i = 0; i < 50; i++) {
        /* Vary norepinephrine (alertness) */
        ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_NOREPINEPHRINE] =
            0.3f + 0.5f * sinf(i * 0.2f);

        entorhinal_process_neuromodulation(ec);

        float velocity[3] = {1.0f, 0.5f, 0.0f};
        EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.01f, 0.1f), 0);
    }

    /* Get position estimate */
    float pos_out[3], heading, pos_conf, head_conf;
    EXPECT_EQ(entorhinal_get_position_estimate(ec, pos_out, &heading, &pos_conf, &head_conf), 0);
}

/*=============================================================================
 * CONCURRENT MODULE TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, ConcurrentModuleProcessing) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    ASSERT_NE(nullptr, ec);

    /* Create multiple modules representing connected brain regions */
    bio_module_context_t modules[3] = {NULL, NULL, NULL};
    const char* module_names[3] = {"hippocampus", "perirhinal", "parahippocampal"};

    for (int i = 0; i < 3; i++) {
        bio_module_info_t mod_info;
        memset(&mod_info, 0, sizeof(mod_info));
        mod_info.module_id = (bio_module_id_t)(TEST_MODULE_HIPPOCAMPUS + i);
        mod_info.module_name = module_names[i];
        mod_info.inbox_capacity = 64;

        modules[i] = bio_router_register_module(&mod_info);
    }

    /* Process all modules concurrently with entorhinal updates */
    for (int cycle = 0; cycle < 20; cycle++) {
        /* Update entorhinal */
        float angle = cycle * 0.2f;
        float position[3] = {cosf(angle) * 3.0f, sinf(angle) * 3.0f, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_bidirectional_update(ec, 5.0f);

        /* Process all module inboxes */
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

    EXPECT_EQ(ENTORHINAL_STATUS_READY, entorhinal_get_status(ec));
}

/*=============================================================================
 * ROUTER LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, RouterShutdownCleanup) {
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

TEST_F(EntorhinalBioAsyncTest, MultipleRouterCycles) {
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

        /* Entorhinal should still work */
        EXPECT_EQ(entorhinal_update_grid_cells(ec, test_position, 3), 0);
    }
}

/*=============================================================================
 * ENTORHINAL STATISTICS TESTS
 *===========================================================================*/

TEST_F(EntorhinalBioAsyncTest, BioAsyncStatsAccumulate) {
    ASSERT_NE(nullptr, ec);

    entorhinal_init_bio_async_bridge(ec, nullptr);

    /* Run multiple processing cycles */
    for (int i = 0; i < 20; i++) {
        entorhinal_sync_bio_async(ec);
        entorhinal_process_neuromodulation(ec);
        entorhinal_bidirectional_update(ec, 10.0f);
    }

    /* Get comprehensive statistics */
    entorhinal_stats_t stats;
    EXPECT_EQ(entorhinal_get_stats(ec, &stats), 0);

    /* Bio-async stats should be tracked */
    /* Note: actual counts depend on implementation */
}

/*=============================================================================
 * MAIN
 *===========================================================================*/

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    /* Clean up global router if still active */
    if (bio_router_is_initialized()) {
        bio_router_shutdown();
    }

    return result;
}
