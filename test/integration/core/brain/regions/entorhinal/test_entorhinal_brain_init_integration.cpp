/**
 * @file test_entorhinal_brain_init_integration.cpp
 * @brief Integration tests for Entorhinal Cortex brain initialization system
 *
 * WHAT: Tests Entorhinal Cortex integration with brain factory initialization,
 *       covering lifecycle management, phase transitions, bridge setup,
 *       component registration, health monitoring, and error handling.
 *
 * WHY:  The entorhinal cortex is the primary interface between hippocampus
 *       and neocortex. Proper initialization is critical for:
 *       - Correct dependency ordering (grid cells before path integration)
 *       - Resource allocation and validation
 *       - Bridge connection sequencing
 *       - Graceful startup and shutdown
 *       - Health monitoring from first activation
 *
 * HOW:  Test initialization phases, bridge connections, KG registration,
 *       factory integration, health monitoring, and error conditions.
 *
 * INTEGRATION POINTS:
 * - Brain factory registration
 * - Brain Knowledge Graph (KG) wiring
 * - Bio-async bridge initialization
 * - Security registration
 * - Health monitoring setup
 * - Phase transition callbacks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_brain_init_bridge.h"
}

/*=============================================================================
 * TEST CALLBACKS
 *===========================================================================*/

static bool g_phase_callback_invoked = false;
static entorhinal_init_phase_t g_last_old_phase = ENTORHINAL_INIT_PHASE_NONE;
static entorhinal_init_phase_t g_last_new_phase = ENTORHINAL_INIT_PHASE_NONE;
static int g_phase_callback_count = 0;

static bool g_progress_callback_invoked = false;
static float g_last_progress = 0.0f;
static int g_progress_callback_count = 0;

static bool g_error_callback_invoked = false;
static int g_last_error_code = 0;
static int g_error_callback_count = 0;

static void test_phase_callback(
    entorhinal_init_phase_t old_phase,
    entorhinal_init_phase_t new_phase,
    void* user_data)
{
    (void)user_data;
    g_phase_callback_invoked = true;
    g_last_old_phase = old_phase;
    g_last_new_phase = new_phase;
    g_phase_callback_count++;
}

static void test_progress_callback(
    entorhinal_init_phase_t phase,
    float progress,
    const char* message,
    void* user_data)
{
    (void)phase;
    (void)message;
    (void)user_data;
    g_progress_callback_invoked = true;
    g_last_progress = progress;
    g_progress_callback_count++;
}

static void test_error_callback(
    entorhinal_init_phase_t phase,
    int error_code,
    const char* error_message,
    void* user_data)
{
    (void)phase;
    (void)error_message;
    (void)user_data;
    g_error_callback_invoked = true;
    g_last_error_code = error_code;
    g_error_callback_count++;
}

static void reset_callbacks(void) {
    g_phase_callback_invoked = false;
    g_last_old_phase = ENTORHINAL_INIT_PHASE_NONE;
    g_last_new_phase = ENTORHINAL_INIT_PHASE_NONE;
    g_phase_callback_count = 0;

    g_progress_callback_invoked = false;
    g_last_progress = 0.0f;
    g_progress_callback_count = 0;

    g_error_callback_invoked = false;
    g_last_error_code = 0;
    g_error_callback_count = 0;
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class EntorhinalBrainInitTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* entorhinal;
    entorhinal_config_t ec_config;
    entorhinal_brain_init_bridge_t* init_bridge;
    entorhinal_brain_init_config_t init_config;

    void SetUp() override {
        entorhinal = NULL;
        init_bridge = NULL;

        /* Get default configurations */
        ec_config = entorhinal_default_config();
        init_config = entorhinal_brain_init_default_config();

        /* Reset test callbacks */
        reset_callbacks();
    }

    void TearDown() override {
        if (init_bridge) {
            entorhinal_brain_init_bridge_destroy(init_bridge);
            init_bridge = NULL;
        }
        if (entorhinal) {
            entorhinal_destroy(entorhinal);
            entorhinal = NULL;
        }
    }

    /* Helper: create entorhinal with default config */
    nimcp_entorhinal_t* createEntorhinal(const entorhinal_config_t* cfg = NULL) {
        return entorhinal_create(cfg ? cfg : &ec_config);
    }

    /* Helper: create init bridge with default config */
    entorhinal_brain_init_bridge_t* createInitBridge(
        const entorhinal_brain_init_config_t* cfg = NULL)
    {
        return entorhinal_brain_init_bridge_create(cfg ? cfg : &init_config);
    }
};

/*=============================================================================
 * LIFECYCLE TESTS - Entorhinal Core
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, CreateEntorhinalWithDefaultConfig) {
    entorhinal = entorhinal_create(&ec_config);
    ASSERT_NE(nullptr, entorhinal);

    /* Verify status */
    entorhinal_status_t status = entorhinal_get_status(entorhinal);
    EXPECT_EQ(ENTORHINAL_STATUS_IDLE, status);
}

TEST_F(EntorhinalBrainInitTest, CreateEntorhinalWithCustomConfig) {
    ec_config.num_grid_cells = 256;
    ec_config.num_border_cells = 64;
    ec_config.num_hd_cells = 30;
    ec_config.enable_path_integration = true;
    ec_config.learning_rate = 0.01f;

    entorhinal = entorhinal_create(&ec_config);
    ASSERT_NE(nullptr, entorhinal);

    /* Verify config was applied */
    entorhinal_config_t applied_config;
    int result = entorhinal_get_config(entorhinal, &applied_config);
    EXPECT_EQ(0, result);
    EXPECT_EQ(256u, applied_config.num_grid_cells);
    EXPECT_EQ(64u, applied_config.num_border_cells);
    EXPECT_EQ(30u, applied_config.num_hd_cells);
    EXPECT_TRUE(applied_config.enable_path_integration);
    EXPECT_FLOAT_EQ(0.01f, applied_config.learning_rate);
}

TEST_F(EntorhinalBrainInitTest, DestroyEntorhinalNull) {
    /* Should not crash */
    entorhinal_destroy(NULL);
}

TEST_F(EntorhinalBrainInitTest, ResetEntorhinal) {
    entorhinal = entorhinal_create(&ec_config);
    ASSERT_NE(nullptr, entorhinal);

    bool result = entorhinal_reset(entorhinal);
    EXPECT_TRUE(result);

    entorhinal_status_t status = entorhinal_get_status(entorhinal);
    EXPECT_EQ(ENTORHINAL_STATUS_IDLE, status);
}

TEST_F(EntorhinalBrainInitTest, MultipleEntorhinalCreateDestroyCycles) {
    for (int i = 0; i < 5; i++) {
        entorhinal = entorhinal_create(&ec_config);
        ASSERT_NE(nullptr, entorhinal) << "Cycle " << i << " create failed";

        entorhinal_destroy(entorhinal);
        entorhinal = NULL;
    }
}

/*=============================================================================
 * LIFECYCLE TESTS - Init Bridge
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, CreateInitBridgeWithDefaultConfig) {
    init_bridge = entorhinal_brain_init_bridge_create(&init_config);
    ASSERT_NE(nullptr, init_bridge);

    entorhinal_init_phase_t phase = entorhinal_brain_init_get_phase(init_bridge);
    EXPECT_EQ(ENTORHINAL_INIT_PHASE_NONE, phase);
}

TEST_F(EntorhinalBrainInitTest, CreateInitBridgeWithCustomConfig) {
    init_config.async_initialization = false;
    init_config.fail_fast = true;
    init_config.skip_self_test = true;
    init_config.init_timeout_ms = 5000;
    init_config.retry_count = 3;

    init_bridge = entorhinal_brain_init_bridge_create(&init_config);
    ASSERT_NE(nullptr, init_bridge);

    /* Verify config was applied by checking behavior */
    EXPECT_FALSE(entorhinal_brain_init_is_ready(init_bridge));
}

TEST_F(EntorhinalBrainInitTest, DestroyInitBridgeNull) {
    /* Should not crash */
    entorhinal_brain_init_bridge_destroy(NULL);
}

TEST_F(EntorhinalBrainInitTest, MultipleInitBridgeCreateDestroyCycles) {
    for (int i = 0; i < 5; i++) {
        init_bridge = entorhinal_brain_init_bridge_create(&init_config);
        ASSERT_NE(nullptr, init_bridge) << "Cycle " << i << " create failed";

        entorhinal_brain_init_bridge_destroy(init_bridge);
        init_bridge = NULL;
    }
}

/*=============================================================================
 * PHASE TRANSITION TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, InitialPhaseIsNone) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    entorhinal_init_phase_t phase = entorhinal_brain_init_get_phase(init_bridge);
    EXPECT_EQ(ENTORHINAL_INIT_PHASE_NONE, phase);
}

TEST_F(EntorhinalBrainInitTest, ExecutePreInitPhase) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    int result = entorhinal_brain_init_execute_phase(
        init_bridge, ENTORHINAL_INIT_PHASE_PRE_INIT);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, ExecuteResourceAllocPhase) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    /* Execute pre-init first */
    entorhinal_brain_init_execute_phase(
        init_bridge, ENTORHINAL_INIT_PHASE_PRE_INIT);

    int result = entorhinal_brain_init_execute_phase(
        init_bridge, ENTORHINAL_INIT_PHASE_RESOURCE_ALLOC);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, AdvancePhaseSequentially) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    /* Connect entorhinal to bridge first */
    int init_result = entorhinal_brain_init_initialize(
        init_bridge, entorhinal, NULL);
    EXPECT_EQ(0, init_result);

    /* Should advance through phases */
    for (int i = 0; i < 3; i++) {
        int result = entorhinal_brain_init_advance_phase(init_bridge);
        EXPECT_EQ(0, result);
    }
}

TEST_F(EntorhinalBrainInitTest, PhaseStringConversion) {
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_NONE));
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_PRE_INIT));
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_RESOURCE_ALLOC));
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_CORE_INIT));
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_BRIDGE_CONNECT));
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_CALIBRATION));
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_SELF_TEST));
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_REGISTRATION));
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_READY));
    EXPECT_NE(nullptr, entorhinal_brain_init_phase_string(ENTORHINAL_INIT_PHASE_ERROR));
}

/*=============================================================================
 * BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, InitializeWithNullEntorhinal) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    int result = entorhinal_brain_init_initialize(init_bridge, NULL, NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, InitializeWithNullBridge) {
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, entorhinal);

    int result = entorhinal_brain_init_initialize(NULL, entorhinal, NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, InitializeSuccess) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    int result = entorhinal_brain_init_initialize(
        init_bridge, entorhinal, NULL);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, ConnectSecurityBridge) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_connect_bridge(
        init_bridge, BRIDGE_INIT_ORDER_SECURITY);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, ConnectAllBridges) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_connect_all_bridges(init_bridge);
    /* May fail if subsystems not available, but should not crash */
    (void)result;
}

/*=============================================================================
 * REGISTRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, RegisterFactoryNull) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    int result = entorhinal_brain_init_register_factory(init_bridge, NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, RegisterKGNull) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    int result = entorhinal_brain_init_register_kg(init_bridge, NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, DeregisterBeforeRegister) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    /* Should handle gracefully even if not registered */
    int result = entorhinal_brain_init_deregister(init_bridge);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * SELF-TEST API TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, SelfTestNull) {
    int result = entorhinal_brain_init_self_test(NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, SelfTestAfterInit) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_self_test(init_bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, TestGridCells) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_test_grid_cells(init_bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, TestPathIntegration) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_test_path_integration(init_bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, TestMemoryGateway) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_test_memory_gateway(init_bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, TestBridges) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_test_bridges(init_bridge);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * CALIBRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, CalibrateNull) {
    int result = entorhinal_brain_init_calibrate(NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, CalibrateAfterInit) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_calibrate(init_bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, CalibrateGridCells) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_calibrate_grid_cells(init_bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, CalibrateHDCells) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_calibrate_hd_cells(init_bridge);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * STATUS API TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, GetStatusNull) {
    entorhinal_init_status_t status;
    int result = entorhinal_brain_init_get_status(NULL, &status);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetStatusNullOutput) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    int result = entorhinal_brain_init_get_status(init_bridge, NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetStatusAfterCreate) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    entorhinal_init_status_t status;
    int result = entorhinal_brain_init_get_status(init_bridge, &status);
    EXPECT_EQ(0, result);
    EXPECT_EQ(ENTORHINAL_INIT_PHASE_NONE, status.current_phase);
    EXPECT_FALSE(status.init_failed);
}

TEST_F(EntorhinalBrainInitTest, IsReadyBeforeInit) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    bool ready = entorhinal_brain_init_is_ready(init_bridge);
    EXPECT_FALSE(ready);
}

TEST_F(EntorhinalBrainInitTest, IsReadyNull) {
    bool ready = entorhinal_brain_init_is_ready(NULL);
    EXPECT_FALSE(ready);
}

TEST_F(EntorhinalBrainInitTest, HasFailedBeforeInit) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    bool failed = entorhinal_brain_init_has_failed(init_bridge);
    EXPECT_FALSE(failed);
}

TEST_F(EntorhinalBrainInitTest, HasFailedNull) {
    bool failed = entorhinal_brain_init_has_failed(NULL);
    EXPECT_FALSE(failed);
}

TEST_F(EntorhinalBrainInitTest, GetError) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    int error_code = 0;
    char error_msg[256] = {0};
    int result = entorhinal_brain_init_get_error(
        init_bridge, &error_code, error_msg, sizeof(error_msg));
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * HEALTH MONITORING TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, GetHealthNull) {
    float health = entorhinal_brain_init_get_health(NULL);
    EXPECT_FLOAT_EQ(0.0f, health);
}

TEST_F(EntorhinalBrainInitTest, GetHealthAfterCreate) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    float health = entorhinal_brain_init_get_health(init_bridge);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(EntorhinalBrainInitTest, UpdateHealthNull) {
    int result = entorhinal_brain_init_update_health(NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, UpdateHealthAfterInit) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_update_health(init_bridge);
    EXPECT_EQ(0, result);

    float health = entorhinal_brain_init_get_health(init_bridge);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(EntorhinalBrainInitTest, ReportHealthNull) {
    int result = entorhinal_brain_init_report_health(NULL);
    EXPECT_NE(0, result);
}

/*=============================================================================
 * SHUTDOWN TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, ShutdownNull) {
    int result = entorhinal_brain_init_shutdown(NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, ShutdownAfterInit) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_shutdown(init_bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, ExecuteShutdownPhase) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_execute_shutdown_phase(
        init_bridge, ENTORHINAL_SHUTDOWN_PHASE_PREPARE);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, ForceShutdownNull) {
    int result = entorhinal_brain_init_force_shutdown(NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, ForceShutdownAfterInit) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_force_shutdown(init_bridge);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * CALLBACK CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, PhaseCallbackConfiguration) {
    init_config.phase_callback = test_phase_callback;
    init_config.callback_user_data = (void*)0xDEADBEEF;

    init_bridge = entorhinal_brain_init_bridge_create(&init_config);
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    /* Phase transitions should invoke callback */
    entorhinal_brain_init_advance_phase(init_bridge);

    EXPECT_TRUE(g_phase_callback_invoked);
    EXPECT_GT(g_phase_callback_count, 0);
}

TEST_F(EntorhinalBrainInitTest, ProgressCallbackConfiguration) {
    init_config.progress_callback = test_progress_callback;
    init_config.callback_user_data = (void*)0xCAFEBABE;
    init_config.log_init_steps = true;

    init_bridge = entorhinal_brain_init_bridge_create(&init_config);
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    /* Progress should be reported during initialization */
    EXPECT_TRUE(g_progress_callback_invoked);
    EXPECT_GE(g_last_progress, 0.0f);
    EXPECT_LE(g_last_progress, 1.0f);
}

/*=============================================================================
 * DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, LogDiagnosticsNull) {
    int result = entorhinal_brain_init_log_diagnostics(NULL);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, LogDiagnosticsAfterInit) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    int result = entorhinal_brain_init_log_diagnostics(init_bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetTimingReportNull) {
    char report[512];
    int result = entorhinal_brain_init_get_timing_report(NULL, report, sizeof(report));
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetTimingReportNullBuffer) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    int result = entorhinal_brain_init_get_timing_report(init_bridge, NULL, 0);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetTimingReportAfterInit) {
    init_bridge = createInitBridge();
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, init_bridge);
    ASSERT_NE(nullptr, entorhinal);

    entorhinal_brain_init_initialize(init_bridge, entorhinal, NULL);

    char report[512] = {0};
    int result = entorhinal_brain_init_get_timing_report(
        init_bridge, report, sizeof(report));
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * DEPENDENCY TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, CheckDependenciesNull) {
    bool satisfied = entorhinal_brain_init_check_dependencies(NULL, ENTORHINAL_DEP_NONE);
    EXPECT_FALSE(satisfied);
}

TEST_F(EntorhinalBrainInitTest, CheckDependenciesNone) {
    init_bridge = createInitBridge();
    ASSERT_NE(nullptr, init_bridge);

    bool satisfied = entorhinal_brain_init_check_dependencies(
        init_bridge, ENTORHINAL_DEP_NONE);
    EXPECT_TRUE(satisfied);
}

TEST_F(EntorhinalBrainInitTest, WaitDependenciesNull) {
    int result = entorhinal_brain_init_wait_dependencies(
        NULL, ENTORHINAL_DEP_MEMORY_POOL, 1000);
    EXPECT_NE(0, result);
}

/*=============================================================================
 * DEFAULT CONFIG TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, DefaultConfigReasonableValues) {
    entorhinal_brain_init_config_t def = entorhinal_brain_init_default_config();

    /* Timeouts should be positive */
    EXPECT_GT(def.init_timeout_ms, 0u);
    EXPECT_GT(def.dependency_timeout_ms, 0u);
    EXPECT_GT(def.bridge_init_timeout_ms, 0u);

    /* Retry should be reasonable */
    EXPECT_GE(def.retry_count, 0u);
    EXPECT_LE(def.retry_count, 10u);
}

TEST_F(EntorhinalBrainInitTest, DefaultEntorhinalConfigReasonableValues) {
    entorhinal_config_t def = entorhinal_default_config();

    /* Grid cell parameters */
    EXPECT_GT(def.num_grid_cells, 0u);
    EXPECT_GT(def.num_border_cells, 0u);
    EXPECT_GT(def.num_hd_cells, 0u);

    /* Spatial parameters */
    EXPECT_GT(def.spatial_dim, 0u);
    EXPECT_GT(def.feature_dim, 0.0f);

    /* Learning rate */
    EXPECT_GT(def.learning_rate, 0.0f);
    EXPECT_LE(def.learning_rate, 1.0f);

    /* Oscillation parameters - theta is 4-12 Hz biologically */
    EXPECT_GE(def.theta_frequency, 4.0f);
    EXPECT_LE(def.theta_frequency, 12.0f);

    /* Gamma is 30-100 Hz biologically */
    EXPECT_GE(def.gamma_frequency, 30.0f);
    EXPECT_LE(def.gamma_frequency, 100.0f);
}

/*=============================================================================
 * ENTORHINAL STATUS AND ERROR TESTS
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, EntorhinalStatusString) {
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_IDLE));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_PATH_INTEGRATING));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_ENCODING));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_RETRIEVING));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_GATEWAY_TRANSFER));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_CONSOLIDATING));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_CALIBRATING));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_READY));
    EXPECT_NE(nullptr, entorhinal_status_string(ENTORHINAL_STATUS_ERROR));
}

TEST_F(EntorhinalBrainInitTest, EntorhinalErrorString) {
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_NONE));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_INVALID_INPUT));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_GRID_DRIFT));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_PATH_INTEGRATION_FAILURE));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_MEMORY_GATEWAY_BLOCKED));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_SECURITY_VIOLATION));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_IMMUNE_REJECTION));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_SUBSTRATE_DEPLETED));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_SYNC_FAILURE));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_BUFFER_OVERFLOW));
    EXPECT_NE(nullptr, entorhinal_error_string(ENTORHINAL_ERROR_INTERNAL));
}

TEST_F(EntorhinalBrainInitTest, GetHealthStatus) {
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, entorhinal);

    float health = entorhinal_get_health_status(entorhinal);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(EntorhinalBrainInitTest, GetHealthStatusNull) {
    float health = entorhinal_get_health_status(NULL);
    EXPECT_FLOAT_EQ(0.0f, health);
}

/*=============================================================================
 * GRID CELL API TESTS (Post-Init)
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, UpdateGridCellsNull) {
    float position[3] = {1.0f, 2.0f, 3.0f};
    int result = entorhinal_update_grid_cells(NULL, position, 3);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, UpdateGridCellsAfterInit) {
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, entorhinal);

    float position[3] = {1.0f, 2.0f, 0.0f};
    int result = entorhinal_update_grid_cells(entorhinal, position, 3);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetGridCellAfterInit) {
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, entorhinal);

    const nimcp_grid_cell_t* cell = entorhinal_get_grid_cell(entorhinal, 0, 0);
    /* May be NULL if no grid modules, but should not crash */
    (void)cell;
}

/*=============================================================================
 * PATH INTEGRATION API TESTS (Post-Init)
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, PathIntegrateNull) {
    float velocity[3] = {0.5f, 0.0f, 0.0f};
    int result = entorhinal_path_integrate(NULL, velocity, 0.0f, 0.01f);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, PathIntegrateAfterInit) {
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, entorhinal);

    float velocity[3] = {0.5f, 0.0f, 0.0f};
    int result = entorhinal_path_integrate(entorhinal, velocity, 0.0f, 0.01f);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetPositionEstimateNull) {
    float position[3], heading, pos_conf, head_conf;
    int result = entorhinal_get_position_estimate(NULL, position, &heading, &pos_conf, &head_conf);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetPositionEstimateAfterInit) {
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, entorhinal);

    float position[3], heading, pos_conf, head_conf;
    int result = entorhinal_get_position_estimate(entorhinal, position, &heading, &pos_conf, &head_conf);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * MEMORY GATEWAY API TESTS (Post-Init)
 *===========================================================================*/

TEST_F(EntorhinalBrainInitTest, SetEncodingGateNull) {
    int result = entorhinal_set_encoding_gate(NULL, 0.5f);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, SetEncodingGateAfterInit) {
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, entorhinal);

    int result = entorhinal_set_encoding_gate(entorhinal, 0.8f);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, SetRetrievalGateNull) {
    int result = entorhinal_set_retrieval_gate(NULL, 0.5f);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, SetRetrievalGateAfterInit) {
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, entorhinal);

    int result = entorhinal_set_retrieval_gate(entorhinal, 0.7f);
    EXPECT_EQ(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetGatewayStatsNull) {
    uint64_t encoded, retrieved, consolidated;
    int result = entorhinal_get_gateway_stats(NULL, &encoded, &retrieved, &consolidated);
    EXPECT_NE(0, result);
}

TEST_F(EntorhinalBrainInitTest, GetGatewayStatsAfterInit) {
    entorhinal = createEntorhinal();
    ASSERT_NE(nullptr, entorhinal);

    uint64_t encoded, retrieved, consolidated;
    int result = entorhinal_get_gateway_stats(entorhinal, &encoded, &retrieved, &consolidated);
    EXPECT_EQ(0, result);
    EXPECT_EQ(0u, encoded);
    EXPECT_EQ(0u, retrieved);
    EXPECT_EQ(0u, consolidated);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
