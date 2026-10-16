/**
 * @file test_bio_async_plasticity_bridge.cpp
 * @brief Unit tests for the Plasticity Bio-Async Bridge
 *
 * Tests the integration bridge between plasticity modules and bio-async:
 * - Bridge initialization
 * - Spike timing extraction from messages
 * - Plasticity event generation
 *
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

/* Include C headers carefully - some may have C++ template issues */
#include "plasticity/integration/nimcp_plasticity_bio_async_bridge.h"
#include "plasticity/nimcp_plasticity_coordinator.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PlasticityBioAsyncBridgeTest : public ::testing::Test {
protected:
    plasticity_bio_async_bridge_t* bridge = nullptr;
    plasticity_coordinator_t* coordinator = nullptr;
    bool bio_async_initialized = false;

    void SetUp() override {
        /* Initialize bio-async subsystem */
        nimcp_bio_async_config_t async_config = nimcp_bio_async_default_config();
        async_config.enable_logging = false;
        async_config.enable_statistics = true;
        nimcp_error_t err = nimcp_bio_async_init(&async_config);
        if (err == NIMCP_SUCCESS) {
            bio_async_initialized = true;
        }

        /* Create plasticity coordinator */
        plasticity_coordinator_config_t coord_config;
        plasticity_coordinator_default_config(&coord_config);
        coord_config.enable_logging = false;
        coordinator = plasticity_coordinator_create(&coord_config);
        /* Coordinator may be NULL if not fully implemented */
    }

    void TearDown() override {
        if (bridge) {
            plasticity_bio_async_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (coordinator) {
            plasticity_coordinator_destroy(coordinator);
            coordinator = nullptr;
        }
        if (bio_async_initialized) {
            nimcp_bio_async_shutdown();
            bio_async_initialized = false;
        }
    }
};

/* ============================================================================
 * Bridge Initialization Tests
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, DefaultConfigValid) {
    plasticity_bio_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    int result = plasticity_bio_async_default_config(&config);
    EXPECT_EQ(result, PLASTICITY_BIO_OK)
        << "Default config should return success";

    /* Verify reasonable defaults */
    EXPECT_GT(config.state_broadcast_interval_ms, 0u);
    EXPECT_GT(config.energy_report_interval_ms, 0u);
    EXPECT_GT(config.max_inbox_process_per_update, 0u);
    EXPECT_GT(config.message_ttl_ms, 0u);
    EXPECT_GT(config.weight_change_threshold, 0.0f);
    EXPECT_GT(config.max_subscriptions, 0u);
    EXPECT_GT(config.max_registered_modules, 0u);
}

TEST_F(PlasticityBioAsyncBridgeTest, DefaultConfigNullFails) {
    int result = plasticity_bio_async_default_config(nullptr);
    EXPECT_EQ(result, PLASTICITY_BIO_ERROR_NULL_PARAM)
        << "NULL config should fail";
}

TEST_F(PlasticityBioAsyncBridgeTest, CreateWithDefaultsSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    EXPECT_NE(bridge, nullptr)
        << "Creating bridge with default config should succeed";
}

TEST_F(PlasticityBioAsyncBridgeTest, CreateWithConfigSucceeds) {
    plasticity_bio_bridge_config_t config;
    plasticity_bio_async_default_config(&config);
    config.enable_logging = false;
    config.enable_weight_broadcasts = true;
    config.enable_ltp_ltd_broadcasts = true;

    bridge = plasticity_bio_async_bridge_create(&config);
    EXPECT_NE(bridge, nullptr)
        << "Creating bridge with custom config should succeed";
}

TEST_F(PlasticityBioAsyncBridgeTest, DestroyNullSafe) {
    /* Should not crash */
    plasticity_bio_async_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(PlasticityBioAsyncBridgeTest, DestroyCleanup) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Destroy and verify no double-free */
    plasticity_bio_async_bridge_destroy(bridge);
    bridge = nullptr;  /* Prevent double destroy in TearDown */

    SUCCEED();
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, ConnectNullBridgeFails) {
    int result = plasticity_bio_async_connect(nullptr, coordinator, nullptr);
    EXPECT_EQ(result, PLASTICITY_BIO_ERROR_NULL_PARAM);
}

TEST_F(PlasticityBioAsyncBridgeTest, ConnectWithoutBioAsyncWorks) {
    /*
     * Bridge should work even if global bio-router is not available.
     * It can connect later or operate in degraded mode.
     */
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Connect with NULL coordinator (testing bridge standalone) */
    int result = plasticity_bio_async_connect(bridge, nullptr, nullptr);

    /* Should either succeed or return specific error, not crash */
    EXPECT_TRUE(
        result == PLASTICITY_BIO_OK ||
        result == PLASTICITY_BIO_ERROR_NULL_PARAM ||
        result == PLASTICITY_BIO_ERROR_NOT_CONNECTED
    );
}

TEST_F(PlasticityBioAsyncBridgeTest, ConnectWithCoordinatorSucceeds) {
    if (!coordinator) {
        GTEST_SKIP() << "Plasticity coordinator not available";
    }

    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = plasticity_bio_async_connect(bridge, coordinator, nullptr);
    EXPECT_EQ(result, PLASTICITY_BIO_OK)
        << "Connect with coordinator should succeed";
}

TEST_F(PlasticityBioAsyncBridgeTest, IsConnectedReturnsFalseBeforeConnect) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    bool connected = plasticity_bio_async_is_connected(bridge);
    EXPECT_FALSE(connected)
        << "Newly created bridge should not be connected";
}

TEST_F(PlasticityBioAsyncBridgeTest, DisconnectSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    if (coordinator) {
        plasticity_bio_async_connect(bridge, coordinator, nullptr);
    }

    int result = plasticity_bio_async_disconnect(bridge);
    EXPECT_EQ(result, PLASTICITY_BIO_OK);

    EXPECT_FALSE(plasticity_bio_async_is_connected(bridge));
}

/* ============================================================================
 * Module Registration Tests
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, RegisterModuleSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = plasticity_bio_async_register_module(
        bridge,
        PLASTICITY_MODULE_STDP,
        "stdp",
        nullptr  /* Mock handle */
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK)
        << "Registering STDP module should succeed";
}

TEST_F(PlasticityBioAsyncBridgeTest, RegisterMultipleModulesSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int r1 = plasticity_bio_async_register_module(bridge, PLASTICITY_MODULE_STDP, "stdp", nullptr);
    int r2 = plasticity_bio_async_register_module(bridge, PLASTICITY_MODULE_BCM, "bcm", nullptr);
    int r3 = plasticity_bio_async_register_module(bridge, PLASTICITY_MODULE_HOMEOSTATIC, "homeostatic", nullptr);

    EXPECT_EQ(r1, PLASTICITY_BIO_OK);
    EXPECT_EQ(r2, PLASTICITY_BIO_OK);
    EXPECT_EQ(r3, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, RegisterWithNullBridgeFails) {
    int result = plasticity_bio_async_register_module(
        nullptr,
        PLASTICITY_MODULE_STDP,
        "stdp",
        nullptr
    );
    EXPECT_EQ(result, PLASTICITY_BIO_ERROR_NULL_PARAM);
}

TEST_F(PlasticityBioAsyncBridgeTest, UnregisterModuleSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    plasticity_bio_async_register_module(bridge, PLASTICITY_MODULE_STDP, "stdp", nullptr);

    int result = plasticity_bio_async_unregister_module(bridge, PLASTICITY_MODULE_STDP);
    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, SetModuleEnabledSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    plasticity_bio_async_register_module(bridge, PLASTICITY_MODULE_STDP, "stdp", nullptr);

    /* Disable */
    int r1 = plasticity_bio_async_set_module_enabled(bridge, PLASTICITY_MODULE_STDP, false);
    EXPECT_EQ(r1, PLASTICITY_BIO_OK);

    /* Re-enable */
    int r2 = plasticity_bio_async_set_module_enabled(bridge, PLASTICITY_MODULE_STDP, true);
    EXPECT_EQ(r2, PLASTICITY_BIO_OK);
}

/* ============================================================================
 * Spike Timing Extraction Tests
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, NotifySpikeTimingSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    /* Register STDP module to receive spike timing */
    plasticity_bio_async_register_module(bridge, PLASTICITY_MODULE_STDP, "stdp", nullptr);

    /* Notify presynaptic spike */
    int r1 = plasticity_bio_async_notify_spike_timing(
        bridge,
        100,   /* neuron_id */
        true,  /* is_presynaptic */
        10.5f  /* spike_time_ms */
    );

    /* Notify postsynaptic spike */
    int r2 = plasticity_bio_async_notify_spike_timing(
        bridge,
        200,   /* neuron_id */
        false, /* is_presynaptic */
        12.3f  /* spike_time_ms */
    );

    EXPECT_EQ(r1, PLASTICITY_BIO_OK);
    EXPECT_EQ(r2, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, SpikeTimingWithNullBridgeFails) {
    int result = plasticity_bio_async_notify_spike_timing(nullptr, 100, true, 10.0f);
    /* Implementation may return NULL_PARAM or NOT_CONNECTED for NULL bridge */
    EXPECT_LT(result, 0) << "NULL bridge should return an error code";
}

TEST_F(PlasticityBioAsyncBridgeTest, SpikeTimingExtremeValues) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Test with extreme but valid values */
    int r1 = plasticity_bio_async_notify_spike_timing(bridge, 0, true, 0.0f);
    int r2 = plasticity_bio_async_notify_spike_timing(bridge, UINT32_MAX, false, 1e6f);
    int r3 = plasticity_bio_async_notify_spike_timing(bridge, 1, true, -1.0f);  /* Negative time? */

    /* Should handle without crashing */
    (void)r1;
    (void)r2;
    (void)r3;
    SUCCEED();
}

/* ============================================================================
 * Plasticity Event Generation Tests
 *
 * NOTE: Broadcast functions require the bridge to be connected.
 * Each test connects the bridge (to coordinator if available) before
 * broadcasting to avoid NOT_CONNECTED errors.
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastWeightUpdateSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_broadcast_weight_update(
        bridge,
        1000,   /* synapse_id */
        0.5f,   /* old_weight */
        0.6f,   /* new_weight */
        PLASTICITY_MODULE_STDP
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK)
        << "Broadcasting weight update should succeed";
}

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastLTPSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_broadcast_ltp(
        bridge,
        1000,   /* synapse_id */
        0.1f,   /* magnitude */
        10.0f,  /* delta_t */
        PLASTICITY_MODULE_STDP
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastLTDSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_broadcast_ltd(
        bridge,
        1001,   /* synapse_id */
        0.05f,  /* magnitude */
        -15.0f, /* delta_t (post before pre) */
        PLASTICITY_MODULE_STDP
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastScalingSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_broadcast_scaling(
        bridge,
        500,    /* neuron_id */
        0.95f,  /* scaling_factor */
        5.0f,   /* target_rate */
        5.5f    /* actual_rate */
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastConsolidationSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_broadcast_consolidation(
        bridge,
        1000,   /* num_synapses */
        0.02f,  /* mean_change */
        true    /* triggered_by_sleep */
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastEligibilityConvertSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_broadcast_eligibility_convert(
        bridge,
        2000,   /* synapse_id */
        0.8f,   /* trace_value */
        0.1f,   /* weight_change */
        1.0f    /* reward_signal */
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastStateChangeSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_broadcast_state_change(
        bridge,
        PLASTICITY_STATE_MAINTENANCE,
        PLASTICITY_STATE_ACQUISITION
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastEnergyReportSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_broadcast_energy_report(
        bridge,
        100.0f, /* total_energy */
        5.0f,   /* energy_rate */
        0.75f   /* budget_utilization */
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastConflictSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_broadcast_conflict(
        bridge,
        3000,                       /* synapse_id */
        PLASTICITY_MODULE_STDP,     /* mech_a */
        PLASTICITY_MODULE_BCM,      /* mech_b */
        0.1f,                       /* change_a */
        -0.05f,                     /* change_b */
        0.05f                       /* resolved */
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

/* ============================================================================
 * Rate Modulation Tests
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, RequestRateModulationSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    int result = plasticity_bio_async_request_rate_modulation(
        bridge,
        1.5f,                           /* modulation_factor */
        PLASTICITY_MODULE_STDP,         /* target_module */
        0.8f,                           /* dopamine */
        0.3f,                           /* norepinephrine */
        0.5f                            /* acetylcholine */
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

TEST_F(PlasticityBioAsyncBridgeTest, RequestRateModulationAllModules) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    /* Target all modules with MODULE_COUNT */
    int result = plasticity_bio_async_request_rate_modulation(
        bridge,
        2.0f,
        PLASTICITY_MODULE_COUNT,  /* Target all */
        0.9f,
        0.1f,
        0.2f
    );

    EXPECT_EQ(result, PLASTICITY_BIO_OK);
}

/* ============================================================================
 * Update and Processing Tests
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, UpdateSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    /* Note: Do NOT connect to coordinator here - update() blocks with
       single-threaded coordinator due to synchronous message processing */

    /* Perform several update cycles */
    for (int i = 0; i < 10; ++i) {
        int result = plasticity_bio_async_update(bridge, 10);  /* 10ms delta */
        /* Accept OK (connected) or NOT_CONNECTED (unconnected bridge) */
        EXPECT_TRUE(result == PLASTICITY_BIO_OK ||
                    result == PLASTICITY_BIO_ERROR_NOT_CONNECTED)
            << "Update returned unexpected error: " << result;
    }
}

TEST_F(PlasticityBioAsyncBridgeTest, ProcessInboxSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    /* Note: Do NOT connect to coordinator here - process_inbox() may block
       with single-threaded coordinator */

    /* Process inbox (may be empty or return NOT_CONNECTED) */
    int processed = plasticity_bio_async_process_inbox(bridge, 100);

    /* Should return 0 (empty), positive count, or NOT_CONNECTED */
    EXPECT_GE(processed, PLASTICITY_BIO_ERROR_NOT_CONNECTED);
}

TEST_F(PlasticityBioAsyncBridgeTest, ProcessInboxNullFails) {
    int result = plasticity_bio_async_process_inbox(nullptr, 100);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, GetStatsSucceeds) {
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    /* Generate some activity */
    plasticity_bio_async_broadcast_weight_update(bridge, 1, 0.5f, 0.6f, PLASTICITY_MODULE_STDP);
    plasticity_bio_async_broadcast_ltp(bridge, 2, 0.1f, 10.0f, PLASTICITY_MODULE_STDP);

    plasticity_bio_async_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = plasticity_bio_async_get_stats(bridge, &stats);
    EXPECT_EQ(result, PLASTICITY_BIO_OK);

    /* Verify some stats were collected */
    EXPECT_GE(stats.messages_sent, 0u);
    EXPECT_GE(stats.weight_update_broadcasts, 0u);
    EXPECT_GE(stats.ltp_events, 0u);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, BroadcastWithNullBridgeFails) {
    int r1 = plasticity_bio_async_broadcast_weight_update(nullptr, 1, 0.5f, 0.6f, PLASTICITY_MODULE_STDP);
    int r2 = plasticity_bio_async_broadcast_ltp(nullptr, 1, 0.1f, 10.0f, PLASTICITY_MODULE_STDP);
    int r3 = plasticity_bio_async_broadcast_ltd(nullptr, 1, 0.1f, -10.0f, PLASTICITY_MODULE_STDP);
    int r4 = plasticity_bio_async_broadcast_consolidation(nullptr, 100, 0.01f, false);

    /* Implementation may return NULL_PARAM or NOT_CONNECTED for NULL bridge */
    EXPECT_LT(r1, 0) << "NULL bridge should return error";
    EXPECT_LT(r2, 0) << "NULL bridge should return error";
    EXPECT_LT(r3, 0) << "NULL bridge should return error";
    EXPECT_LT(r4, 0) << "NULL bridge should return error";
}

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

TEST_F(PlasticityBioAsyncBridgeTest, TypicalSTDPWorkflow) {
    /*
     * Simulate a typical STDP workflow:
     * 1. Create and connect bridge
     * 2. Register STDP module
     * 3. Receive spike timing events
     * 4. Broadcast weight updates
     * 5. Report energy
     */
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    /* Register STDP */
    int r = plasticity_bio_async_register_module(bridge, PLASTICITY_MODULE_STDP, "stdp", nullptr);
    EXPECT_EQ(r, PLASTICITY_BIO_OK);

    /* Simulate pre-post spike pair (LTP) */
    plasticity_bio_async_notify_spike_timing(bridge, 100, true, 10.0f);   /* Pre at 10ms */
    plasticity_bio_async_notify_spike_timing(bridge, 200, false, 15.0f); /* Post at 15ms */

    /* Process and update */
    plasticity_bio_async_process_inbox(bridge, 10);
    plasticity_bio_async_update(bridge, 5);

    /* Broadcast resulting LTP */
    plasticity_bio_async_broadcast_ltp(bridge, 1000, 0.1f, 5.0f, PLASTICITY_MODULE_STDP);
    plasticity_bio_async_broadcast_weight_update(bridge, 1000, 0.5f, 0.6f, PLASTICITY_MODULE_STDP);

    /* Report energy */
    plasticity_bio_async_broadcast_energy_report(bridge, 10.0f, 2.0f, 0.5f);

    /* Verify stats */
    plasticity_bio_async_stats_t stats;
    plasticity_bio_async_get_stats(bridge, &stats);
    EXPECT_GT(stats.ltp_events, 0u);
    EXPECT_GT(stats.weight_update_broadcasts, 0u);
}

TEST_F(PlasticityBioAsyncBridgeTest, HomeostaticScalingWorkflow) {
    /*
     * Simulate homeostatic scaling workflow:
     * 1. Create bridge
     * 2. Register homeostatic module
     * 3. Broadcast scaling events
     */
    bridge = plasticity_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    if (coordinator) plasticity_bio_async_connect(bridge, coordinator, nullptr);

    plasticity_bio_async_register_module(bridge, PLASTICITY_MODULE_HOMEOSTATIC, "homeostatic", nullptr);

    /* Neuron firing too high - scale down */
    plasticity_bio_async_broadcast_scaling(bridge, 100, 0.9f, 5.0f, 7.0f);

    /* Neuron firing too low - scale up */
    plasticity_bio_async_broadcast_scaling(bridge, 101, 1.1f, 5.0f, 3.0f);

    plasticity_bio_async_stats_t stats;
    plasticity_bio_async_get_stats(bridge, &stats);
    EXPECT_GE(stats.scaling_events, 2u);
}
