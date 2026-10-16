/**
 * @file test_brain_init_bridge.cpp
 * @brief Unit tests for Entorhinal-Brain Initialization Bridge
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal_brain_init_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

class BrainInitBridgeTest : public ::testing::Test {
protected:
    entorhinal_brain_init_bridge_t* bridge = nullptr;
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_brain_init_config_t config = entorhinal_brain_init_default_config();
        config.skip_self_test = true;  // Speed up tests
        config.skip_calibration = true;
        bridge = entorhinal_brain_init_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        entorhinal_config_t ec_config = entorhinal_default_config();
        ec = entorhinal_create(&ec_config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            entorhinal_brain_init_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, CreateWithDefaultConfig) {
    entorhinal_brain_init_bridge_t* b = entorhinal_brain_init_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_FALSE(b->config.async_initialization);
    EXPECT_TRUE(b->config.fail_fast);
    EXPECT_TRUE(b->config.wait_for_dependencies);
    entorhinal_brain_init_bridge_destroy(b);
}

TEST_F(BrainInitBridgeTest, CreateWithCustomConfig) {
    entorhinal_brain_init_config_t config = entorhinal_brain_init_default_config();
    config.async_initialization = true;
    config.retry_count = 5;

    entorhinal_brain_init_bridge_t* b = entorhinal_brain_init_bridge_create(&config);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->config.async_initialization);
    EXPECT_EQ(b->config.retry_count, 5u);
    entorhinal_brain_init_bridge_destroy(b);
}

TEST_F(BrainInitBridgeTest, DestroyNull) {
    entorhinal_brain_init_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(BrainInitBridgeTest, InitialState) {
    EXPECT_EQ(bridge->status.current_phase, ENTORHINAL_INIT_PHASE_NONE);
    EXPECT_EQ(bridge->status.shutdown_phase, ENTORHINAL_SHUTDOWN_PHASE_NONE);
    EXPECT_FALSE(bridge->status.init_failed);
    EXPECT_EQ(bridge->status.bridges_total, BRIDGE_INIT_ORDER_COUNT);
}

/*=============================================================================
 * INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, InitializeBasic) {
    EXPECT_EQ(entorhinal_brain_init_initialize(bridge, ec, nullptr), 0);
    EXPECT_TRUE(entorhinal_brain_init_is_ready(bridge));
    EXPECT_EQ(bridge->status.current_phase, ENTORHINAL_INIT_PHASE_READY);
}

TEST_F(BrainInitBridgeTest, InitializeNull) {
    EXPECT_EQ(entorhinal_brain_init_initialize(nullptr, ec, nullptr), -1);
    EXPECT_EQ(entorhinal_brain_init_initialize(bridge, nullptr, nullptr), -1);
}

TEST_F(BrainInitBridgeTest, InitializeSetsEntorhinal) {
    entorhinal_brain_init_initialize(bridge, ec, nullptr);
    EXPECT_EQ(bridge->entorhinal, ec);
}

TEST_F(BrainInitBridgeTest, InitializeIncrementsAttempts) {
    uint64_t initial_attempts = bridge->total_init_attempts;
    entorhinal_brain_init_initialize(bridge, ec, nullptr);
    EXPECT_EQ(bridge->total_init_attempts, initial_attempts + 1);
}

TEST_F(BrainInitBridgeTest, InitializeIncrementsSuccesses) {
    uint64_t initial_successes = bridge->successful_inits;
    entorhinal_brain_init_initialize(bridge, ec, nullptr);
    EXPECT_EQ(bridge->successful_inits, initial_successes + 1);
}

/*=============================================================================
 * DEPENDENCY TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, CheckDependenciesMemoryPool) {
    EXPECT_TRUE(entorhinal_brain_init_check_dependencies(bridge, ENTORHINAL_DEP_MEMORY_POOL));
}

TEST_F(BrainInitBridgeTest, CheckDependenciesNull) {
    EXPECT_FALSE(entorhinal_brain_init_check_dependencies(nullptr, ENTORHINAL_DEP_MEMORY_POOL));
}

TEST_F(BrainInitBridgeTest, WaitDependencies) {
    EXPECT_EQ(entorhinal_brain_init_wait_dependencies(bridge,
        ENTORHINAL_DEP_MEMORY_POOL, 1000), 0);
}

TEST_F(BrainInitBridgeTest, WaitDependenciesNull) {
    EXPECT_EQ(entorhinal_brain_init_wait_dependencies(nullptr, 0, 1000), -1);
}

/*=============================================================================
 * PHASE EXECUTION TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, ExecutePreInitPhase) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_execute_phase(bridge, ENTORHINAL_INIT_PHASE_PRE_INIT), 0);
    EXPECT_EQ(bridge->status.current_phase, ENTORHINAL_INIT_PHASE_PRE_INIT);
}

TEST_F(BrainInitBridgeTest, ExecuteResourceAllocPhase) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_execute_phase(bridge, ENTORHINAL_INIT_PHASE_RESOURCE_ALLOC), 0);
}

TEST_F(BrainInitBridgeTest, ExecuteCoreInitPhase) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_execute_phase(bridge, ENTORHINAL_INIT_PHASE_CORE_INIT), 0);
    EXPECT_TRUE(bridge->status.grid_cells_initialized);
    EXPECT_TRUE(bridge->status.border_cells_initialized);
    EXPECT_TRUE(bridge->status.hd_cells_initialized);
    EXPECT_TRUE(bridge->status.path_integration_initialized);
    EXPECT_TRUE(bridge->status.memory_gateway_initialized);
}

TEST_F(BrainInitBridgeTest, ExecuteBridgeConnectPhase) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_execute_phase(bridge, ENTORHINAL_INIT_PHASE_BRIDGE_CONNECT), 0);
    EXPECT_GT(bridge->status.bridges_connected, 0u);
}

TEST_F(BrainInitBridgeTest, ExecutePhaseNull) {
    EXPECT_EQ(entorhinal_brain_init_execute_phase(nullptr, ENTORHINAL_INIT_PHASE_PRE_INIT), -1);
}

TEST_F(BrainInitBridgeTest, AdvancePhase) {
    bridge->entorhinal = ec;
    bridge->status.current_phase = ENTORHINAL_INIT_PHASE_PRE_INIT;
    EXPECT_EQ(entorhinal_brain_init_advance_phase(bridge), 0);
    EXPECT_EQ(bridge->status.current_phase, ENTORHINAL_INIT_PHASE_RESOURCE_ALLOC);
}

/*=============================================================================
 * BRIDGE CONNECTION TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, ConnectSingleBridge) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_connect_bridge(bridge, BRIDGE_INIT_ORDER_SECURITY), 0);
    EXPECT_TRUE(bridge->status.bridges_initialized[BRIDGE_INIT_ORDER_SECURITY]);
    EXPECT_EQ(bridge->status.bridges_connected, 1u);
}

TEST_F(BrainInitBridgeTest, ConnectBridgeNull) {
    EXPECT_EQ(entorhinal_brain_init_connect_bridge(nullptr, BRIDGE_INIT_ORDER_SECURITY), -1);
}

TEST_F(BrainInitBridgeTest, ConnectBridgeInvalid) {
    EXPECT_EQ(entorhinal_brain_init_connect_bridge(bridge, BRIDGE_INIT_ORDER_COUNT), -1);
}

TEST_F(BrainInitBridgeTest, ConnectAllBridges) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_connect_all_bridges(bridge), 0);
    EXPECT_EQ(bridge->status.bridges_connected, BRIDGE_INIT_ORDER_COUNT);
}

TEST_F(BrainInitBridgeTest, ConnectAllBridgesNull) {
    EXPECT_EQ(entorhinal_brain_init_connect_all_bridges(nullptr), -1);
}

/*=============================================================================
 * SHUTDOWN TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, Shutdown) {
    entorhinal_brain_init_initialize(bridge, ec, nullptr);
    EXPECT_EQ(entorhinal_brain_init_shutdown(bridge), 0);
    EXPECT_EQ(bridge->status.shutdown_phase, ENTORHINAL_SHUTDOWN_PHASE_COMPLETE);
}

TEST_F(BrainInitBridgeTest, ShutdownNull) {
    EXPECT_EQ(entorhinal_brain_init_shutdown(nullptr), -1);
}

TEST_F(BrainInitBridgeTest, ForceShutdown) {
    entorhinal_brain_init_initialize(bridge, ec, nullptr);
    EXPECT_EQ(entorhinal_brain_init_force_shutdown(bridge), 0);
    EXPECT_EQ(bridge->status.current_phase, ENTORHINAL_INIT_PHASE_NONE);
}

TEST_F(BrainInitBridgeTest, ForceShutdownNull) {
    EXPECT_EQ(entorhinal_brain_init_force_shutdown(nullptr), -1);
}

TEST_F(BrainInitBridgeTest, ExecuteShutdownPhase) {
    EXPECT_EQ(entorhinal_brain_init_execute_shutdown_phase(bridge,
        ENTORHINAL_SHUTDOWN_PHASE_PREPARE), 0);
    EXPECT_EQ(bridge->status.shutdown_phase, ENTORHINAL_SHUTDOWN_PHASE_PREPARE);
}

/*=============================================================================
 * REGISTRATION TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, RegisterFactory) {
    EXPECT_EQ(entorhinal_brain_init_register_factory(bridge, nullptr), 0);
    EXPECT_TRUE(bridge->registered_with_factory);
}

TEST_F(BrainInitBridgeTest, RegisterFactoryNull) {
    EXPECT_EQ(entorhinal_brain_init_register_factory(nullptr, nullptr), -1);
}

TEST_F(BrainInitBridgeTest, RegisterKG) {
    EXPECT_EQ(entorhinal_brain_init_register_kg(bridge, nullptr), 0);
    EXPECT_TRUE(bridge->registered_with_kg);
}

TEST_F(BrainInitBridgeTest, RegisterKGNull) {
    EXPECT_EQ(entorhinal_brain_init_register_kg(nullptr, nullptr), -1);
}

TEST_F(BrainInitBridgeTest, Deregister) {
    entorhinal_brain_init_register_factory(bridge, nullptr);
    entorhinal_brain_init_register_kg(bridge, nullptr);

    EXPECT_EQ(entorhinal_brain_init_deregister(bridge), 0);
    EXPECT_FALSE(bridge->registered_with_factory);
    EXPECT_FALSE(bridge->registered_with_kg);
}

/*=============================================================================
 * SELF-TEST TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, SelfTest) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_self_test(bridge), 0);
    EXPECT_EQ(bridge->status.self_test_failures, 0u);
}

TEST_F(BrainInitBridgeTest, SelfTestNull) {
    EXPECT_EQ(entorhinal_brain_init_self_test(nullptr), -1);
}

TEST_F(BrainInitBridgeTest, TestGridCells) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_test_grid_cells(bridge), 0);
}

TEST_F(BrainInitBridgeTest, TestPathIntegration) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_test_path_integration(bridge), 0);
}

TEST_F(BrainInitBridgeTest, TestMemoryGateway) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_test_memory_gateway(bridge), 0);
}

TEST_F(BrainInitBridgeTest, TestBridges) {
    bridge->entorhinal = ec;
    entorhinal_brain_init_connect_all_bridges(bridge);
    EXPECT_EQ(entorhinal_brain_init_test_bridges(bridge), 0);
}

/*=============================================================================
 * CALIBRATION TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, Calibrate) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_calibrate(bridge), 0);
}

TEST_F(BrainInitBridgeTest, CalibrateNull) {
    EXPECT_EQ(entorhinal_brain_init_calibrate(nullptr), -1);
}

TEST_F(BrainInitBridgeTest, CalibrateGridCells) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_calibrate_grid_cells(bridge), 0);
}

TEST_F(BrainInitBridgeTest, CalibrateHDCells) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_calibrate_hd_cells(bridge), 0);
}

/*=============================================================================
 * STATUS TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, GetStatus) {
    entorhinal_init_status_t status;
    EXPECT_EQ(entorhinal_brain_init_get_status(bridge, &status), 0);
    EXPECT_EQ(status.current_phase, bridge->status.current_phase);
}

TEST_F(BrainInitBridgeTest, GetStatusNull) {
    entorhinal_init_status_t status;
    EXPECT_EQ(entorhinal_brain_init_get_status(nullptr, &status), -1);
    EXPECT_EQ(entorhinal_brain_init_get_status(bridge, nullptr), -1);
}

TEST_F(BrainInitBridgeTest, GetPhase) {
    EXPECT_EQ(entorhinal_brain_init_get_phase(bridge), ENTORHINAL_INIT_PHASE_NONE);
}

TEST_F(BrainInitBridgeTest, GetPhaseNull) {
    EXPECT_EQ(entorhinal_brain_init_get_phase(nullptr), ENTORHINAL_INIT_PHASE_NONE);
}

TEST_F(BrainInitBridgeTest, IsReady) {
    EXPECT_FALSE(entorhinal_brain_init_is_ready(bridge));
    entorhinal_brain_init_initialize(bridge, ec, nullptr);
    EXPECT_TRUE(entorhinal_brain_init_is_ready(bridge));
}

TEST_F(BrainInitBridgeTest, IsReadyNull) {
    EXPECT_FALSE(entorhinal_brain_init_is_ready(nullptr));
}

TEST_F(BrainInitBridgeTest, HasFailed) {
    EXPECT_FALSE(entorhinal_brain_init_has_failed(bridge));
}

TEST_F(BrainInitBridgeTest, HasFailedNull) {
    EXPECT_TRUE(entorhinal_brain_init_has_failed(nullptr));
}

TEST_F(BrainInitBridgeTest, GetError) {
    int error_code;
    char error_message[256];
    EXPECT_EQ(entorhinal_brain_init_get_error(bridge, &error_code, error_message, 256), 0);
}

TEST_F(BrainInitBridgeTest, GetErrorNull) {
    EXPECT_EQ(entorhinal_brain_init_get_error(nullptr, nullptr, nullptr, 0), -1);
}

TEST_F(BrainInitBridgeTest, PhaseString) {
    EXPECT_STREQ(entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_NONE), "None");
    EXPECT_STREQ(entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_READY), "Ready");
    EXPECT_STREQ(entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_ERROR), "Error");
}

TEST_F(BrainInitBridgeTest, PhaseStringInvalid) {
    EXPECT_STREQ(entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_COUNT), "Unknown");
}

/*=============================================================================
 * HEALTH MONITORING TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, UpdateHealth) {
    bridge->entorhinal = ec;
    EXPECT_EQ(entorhinal_brain_init_update_health(bridge), 0);
}

TEST_F(BrainInitBridgeTest, UpdateHealthNull) {
    EXPECT_EQ(entorhinal_brain_init_update_health(nullptr), -1);
}

TEST_F(BrainInitBridgeTest, GetHealth) {
    float health = entorhinal_brain_init_get_health(bridge);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(BrainInitBridgeTest, GetHealthNull) {
    EXPECT_FLOAT_EQ(entorhinal_brain_init_get_health(nullptr), 0.0f);
}

TEST_F(BrainInitBridgeTest, ReportHealth) {
    EXPECT_EQ(entorhinal_brain_init_report_health(bridge), 0);
}

/*=============================================================================
 * DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(BrainInitBridgeTest, LogDiagnostics) {
    EXPECT_EQ(entorhinal_brain_init_log_diagnostics(bridge), 0);
}

TEST_F(BrainInitBridgeTest, LogDiagnosticsNull) {
    EXPECT_EQ(entorhinal_brain_init_log_diagnostics(nullptr), -1);
}

TEST_F(BrainInitBridgeTest, GetTimingReport) {
    entorhinal_brain_init_initialize(bridge, ec, nullptr);

    char report[1024];
    EXPECT_EQ(entorhinal_brain_init_get_timing_report(bridge, report, 1024), 0);
    EXPECT_TRUE(strlen(report) > 0);
}

TEST_F(BrainInitBridgeTest, GetTimingReportNull) {
    char report[256];
    EXPECT_EQ(entorhinal_brain_init_get_timing_report(nullptr, report, 256), -1);
    EXPECT_EQ(entorhinal_brain_init_get_timing_report(bridge, nullptr, 256), -1);
    EXPECT_EQ(entorhinal_brain_init_get_timing_report(bridge, report, 0), -1);
}

