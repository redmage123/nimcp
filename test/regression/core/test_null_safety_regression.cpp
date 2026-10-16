/**
 * @file test_null_safety_regression.cpp
 * @brief Regression tests for NULL pointer safety in public APIs
 *
 * WHAT: Tests to ensure NULL pointer inputs are handled safely
 * WHY:  Prevent crashes from NULL inputs, ensure proper error returns
 * HOW:  Test all public API functions with NULL parameters
 *
 * BUG HISTORY:
 * - Bug #1: Multiple functions crashed on NULL input
 *   FIX: Added guard clauses with NIMCP_CHECK_NULL macros
 * - Bug #2: NULL safety checks missing in new functions
 *   FIX: Made NULL checks mandatory in code review
 * - Bug #3: Inconsistent error returns (some -1, some NIMCP_ERROR_*)
 *   FIX: Standardized on NIMCP_ERROR_NULL_POINTER for NULL inputs
 *
 * REGRESSION FOCUS:
 * 1. NULL inputs should not crash the program
 * 2. NULL inputs should return NIMCP_ERROR_NULL_POINTER or false
 * 3. All public API functions must be tested
 *
 * @version 1.0.0
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <signal.h>
#include <setjmp.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/error/nimcp_error_codes.h"
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "utils/algorithms/nimcp_monte_carlo.h"

//=============================================================================
// Crash Detection Infrastructure
//=============================================================================

static sigjmp_buf jump_buffer;
static volatile sig_atomic_t did_crash = 0;

static void crash_handler(int sig) {
    (void)sig;
    did_crash = 1;
    siglongjmp(jump_buffer, 1);
}

/**
 * @brief RAII helper to install/restore crash handlers
 */
class CrashGuard {
public:
    CrashGuard() {
        /* Save original handlers */
        orig_sigsegv = signal(SIGSEGV, crash_handler);
        orig_sigabrt = signal(SIGABRT, crash_handler);
        orig_sigbus = signal(SIGBUS, crash_handler);
        did_crash = 0;
    }

    ~CrashGuard() {
        /* Restore original handlers */
        signal(SIGSEGV, orig_sigsegv);
        signal(SIGABRT, orig_sigabrt);
        signal(SIGBUS, orig_sigbus);
    }

    bool crashed() const { return did_crash != 0; }

private:
    void (*orig_sigsegv)(int);
    void (*orig_sigabrt)(int);
    void (*orig_sigbus)(int);
};

/**
 * @brief Macro to test that a function call doesn't crash with NULL
 */
#define EXPECT_NO_CRASH(expr) \
    do { \
        CrashGuard guard; \
        if (sigsetjmp(jump_buffer, 1) == 0) { \
            expr; \
            EXPECT_FALSE(guard.crashed()) << "REGRESSION: " #expr " crashed on NULL input"; \
        } else { \
            FAIL() << "REGRESSION: " #expr " crashed on NULL input"; \
        } \
    } while (0)

//=============================================================================
// Test Fixture
//=============================================================================

class NullSafetyRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Clear any previous error state */
        nimcp_error_clear();
    }

    void TearDown() override {
        nimcp_error_clear();
    }
};

//=============================================================================
// MOTOR CORTEX NULL SAFETY TESTS
//=============================================================================

/**
 * BUG: Motor adapter functions crashed on NULL inputs
 *
 * FIX: Added NIMCP_CHECK_NULL guards at function entry
 */
TEST_F(NullSafetyRegressionTest, Motor_CreateWithNull) {
    /**
     * REGRESSION TEST: motor_create(NULL) should use defaults, not crash
     */
    motor_adapter_t* adapter = nullptr;
    EXPECT_NO_CRASH(adapter = motor_create(nullptr));
    if (adapter) {
        motor_destroy(adapter);
    }
}

TEST_F(NullSafetyRegressionTest, Motor_DestroyWithNull) {
    /**
     * REGRESSION TEST: motor_destroy(NULL) should be safe no-op
     */
    EXPECT_NO_CRASH(motor_destroy(nullptr));
}

TEST_F(NullSafetyRegressionTest, Motor_ResetWithNull) {
    /**
     * REGRESSION TEST: motor_reset(NULL) should return false, not crash
     */
    bool result = true;
    EXPECT_NO_CRASH(result = motor_reset(nullptr));
    EXPECT_FALSE(result) << "REGRESSION: motor_reset(NULL) should return false";
}

TEST_F(NullSafetyRegressionTest, Motor_PlanMovementWithNull) {
    /**
     * REGRESSION TEST: motor_plan_movement with NULL should return false
     */
    motor_adapter_t* adapter = motor_create(nullptr);
    if (!adapter) {
        GTEST_SKIP() << "Could not create motor adapter";
    }

    bool result = true;
    EXPECT_NO_CRASH(result = motor_plan_movement(nullptr, nullptr));
    EXPECT_FALSE(result);

    motor_goal_t goal = {};
    EXPECT_NO_CRASH(result = motor_plan_movement(nullptr, &goal));
    EXPECT_FALSE(result);

    EXPECT_NO_CRASH(result = motor_plan_movement(adapter, nullptr));
    EXPECT_FALSE(result);

    motor_destroy(adapter);
}

TEST_F(NullSafetyRegressionTest, Motor_GetStatusWithNull) {
    /**
     * REGRESSION TEST: motor_get_status(NULL) should return error status
     */
    motor_status_t status;
    EXPECT_NO_CRASH(status = motor_get_status(nullptr));
    EXPECT_EQ(status, MOTOR_STATUS_ERROR) << "REGRESSION: motor_get_status(NULL) should return MOTOR_STATUS_ERROR";
}

TEST_F(NullSafetyRegressionTest, Motor_GetLastErrorWithNull) {
    /**
     * REGRESSION TEST: motor_get_last_error(NULL) should return error
     */
    motor_error_t error;
    EXPECT_NO_CRASH(error = motor_get_last_error(nullptr));
    EXPECT_NE(error, MOTOR_ERROR_NONE) << "REGRESSION: motor_get_last_error(NULL) should return error";
}

TEST_F(NullSafetyRegressionTest, Motor_SetCallbackWithNull) {
    /**
     * REGRESSION TEST: motor_set_*_callback(NULL, ...) should return false
     */
    bool result = true;

    EXPECT_NO_CRASH(result = motor_set_command_callback(nullptr, nullptr, nullptr));
    EXPECT_FALSE(result);

    EXPECT_NO_CRASH(result = motor_set_complete_callback(nullptr, nullptr, nullptr));
    EXPECT_FALSE(result);

    EXPECT_NO_CRASH(result = motor_set_event_callback(nullptr, nullptr, nullptr));
    EXPECT_FALSE(result);
}

TEST_F(NullSafetyRegressionTest, Motor_GetStatsWithNull) {
    /**
     * REGRESSION TEST: motor_get_stats with NULL should return false
     */
    motor_adapter_t* adapter = motor_create(nullptr);
    if (!adapter) {
        GTEST_SKIP() << "Could not create motor adapter";
    }

    bool result = true;
    motor_stats_t stats = {};

    EXPECT_NO_CRASH(result = motor_get_stats(nullptr, &stats));
    EXPECT_FALSE(result);

    EXPECT_NO_CRASH(result = motor_get_stats(adapter, nullptr));
    EXPECT_FALSE(result);

    EXPECT_NO_CRASH(result = motor_get_stats(nullptr, nullptr));
    EXPECT_FALSE(result);

    motor_destroy(adapter);
}

//=============================================================================
// SNN NULL SAFETY TESTS
//=============================================================================

TEST_F(NullSafetyRegressionTest, SNN_CreateWithNull) {
    /**
     * REGRESSION TEST: snn_network_create(NULL) should return NULL, not crash
     */
    snn_network_t* network = nullptr;
    EXPECT_NO_CRASH(network = snn_network_create(nullptr));
    EXPECT_EQ(network, nullptr) << "REGRESSION: snn_network_create(NULL) should return NULL";
}

TEST_F(NullSafetyRegressionTest, SNN_DestroyWithNull) {
    /**
     * REGRESSION TEST: snn_network_destroy(NULL) should be safe no-op
     */
    EXPECT_NO_CRASH(snn_network_destroy(nullptr));
}

TEST_F(NullSafetyRegressionTest, SNN_ResetWithNull) {
    /**
     * REGRESSION TEST: snn_network_reset(NULL) should return error code
     */
    int result = 0;
    EXPECT_NO_CRASH(result = snn_network_reset(nullptr));
    EXPECT_NE(result, 0) << "REGRESSION: snn_network_reset(NULL) should return error";
}

TEST_F(NullSafetyRegressionTest, SNN_StepWithNull) {
    /**
     * REGRESSION TEST: snn_network_step(NULL, ...) should return error
     */
    int result = 0;
    EXPECT_NO_CRASH(result = snn_network_step(nullptr, 1.0f));
    EXPECT_LT(result, 0) << "REGRESSION: snn_network_step(NULL) should return negative error";
}

TEST_F(NullSafetyRegressionTest, SNN_SetInputsWithNull) {
    /**
     * REGRESSION TEST: snn_network_set_inputs with NULL should return error
     */
    float inputs[] = {1.0f, 2.0f, 3.0f};
    int result = 0;

    EXPECT_NO_CRASH(result = snn_network_set_inputs(nullptr, inputs, 3));
    EXPECT_NE(result, 0) << "REGRESSION: snn_network_set_inputs(NULL, ...) should return error";

    /* Create valid network for second test */
    snn_config_t config;
    snn_config_default(&config);
    config.n_inputs = 3;
    config.n_outputs = 2;
    snn_network_t* network = snn_network_create(&config);
    if (network) {
        EXPECT_NO_CRASH(result = snn_network_set_inputs(network, nullptr, 3));
        EXPECT_NE(result, 0) << "REGRESSION: snn_network_set_inputs(..., NULL, ...) should return error";
        snn_network_destroy(network);
    }
}

//=============================================================================
// MONTE CARLO RNG NULL SAFETY TESTS
//=============================================================================

TEST_F(NullSafetyRegressionTest, MonteCarlo_UniformWithNull) {
    /**
     * REGRESSION TEST: mc_random_uniform(NULL) should return 0.0, not crash
     */
    float result = 1.0f;
    EXPECT_NO_CRASH(result = mc_random_uniform(nullptr));
    EXPECT_EQ(result, 0.0f) << "REGRESSION: mc_random_uniform(NULL) should return 0.0";
}

TEST_F(NullSafetyRegressionTest, MonteCarlo_NormalWithNull) {
    /**
     * REGRESSION TEST: mc_random_normal(NULL, ...) should return mean, not crash
     */
    float result = -999.0f;
    float mean = 5.0f;
    EXPECT_NO_CRASH(result = mc_random_normal(nullptr, mean, 1.0f));
    EXPECT_EQ(result, mean) << "REGRESSION: mc_random_normal(NULL) should return mean";
}

//=============================================================================
// ERROR HANDLING NULL SAFETY TESTS
//=============================================================================

TEST_F(NullSafetyRegressionTest, Error_SetWithNull) {
    /**
     * REGRESSION TEST: nimcp_error_set with NULL file/function should not crash
     */
    EXPECT_NO_CRASH(nimcp_error_set(NIMCP_ERROR_UNKNOWN, nullptr, 0, nullptr, nullptr));
    EXPECT_NO_CRASH(nimcp_error_set(NIMCP_ERROR_UNKNOWN, __FILE__, __LINE__, nullptr, nullptr));
    EXPECT_NO_CRASH(nimcp_error_set(NIMCP_ERROR_UNKNOWN, nullptr, __LINE__, __func__, nullptr));
}

TEST_F(NullSafetyRegressionTest, Error_ToStringWithInvalid) {
    /**
     * REGRESSION TEST: nimcp_error_to_string should handle any code
     */
    const char* str = nullptr;

    EXPECT_NO_CRASH(str = nimcp_error_to_string(NIMCP_SUCCESS));
    EXPECT_NE(str, nullptr);

    EXPECT_NO_CRASH(str = nimcp_error_to_string(-1));
    EXPECT_NE(str, nullptr);

    EXPECT_NO_CRASH(str = nimcp_error_to_string(99999));
    EXPECT_NE(str, nullptr);
}

TEST_F(NullSafetyRegressionTest, Error_PrintWithAnyCode) {
    /**
     * REGRESSION TEST: nimcp_error_print should handle any code without crash
     */
    EXPECT_NO_CRASH(nimcp_error_print(NIMCP_SUCCESS));
    EXPECT_NO_CRASH(nimcp_error_print(NIMCP_ERROR_NULL_POINTER));
    EXPECT_NO_CRASH(nimcp_error_print(-1));
    EXPECT_NO_CRASH(nimcp_error_print(99999));
}

TEST_F(NullSafetyRegressionTest, Error_PrintDetailedWithNull) {
    /**
     * REGRESSION TEST: nimcp_error_print_detailed(NULL) should not crash
     */
    EXPECT_NO_CRASH(nimcp_error_print_detailed(nullptr));
}

//=============================================================================
// CLEANUP STACK NULL SAFETY TESTS
//=============================================================================

TEST_F(NullSafetyRegressionTest, CleanupStack_InitWithNull) {
    /**
     * REGRESSION TEST: nimcp_cleanup_init(NULL) should not crash
     */
    EXPECT_NO_CRASH(nimcp_cleanup_init(nullptr));
}

TEST_F(NullSafetyRegressionTest, CleanupStack_PushWithNull) {
    /**
     * REGRESSION TEST: nimcp_cleanup_push with NULL stack should return false
     */
    bool result = true;
    EXPECT_NO_CRASH(result = nimcp_cleanup_push(nullptr, nullptr, nullptr, nullptr));
    EXPECT_FALSE(result);
}

TEST_F(NullSafetyRegressionTest, CleanupStack_ExecuteWithNull) {
    /**
     * REGRESSION TEST: nimcp_cleanup_execute(NULL) should not crash
     */
    EXPECT_NO_CRASH(nimcp_cleanup_execute(nullptr));
}

TEST_F(NullSafetyRegressionTest, CleanupStack_ClearWithNull) {
    /**
     * REGRESSION TEST: nimcp_cleanup_clear(NULL) should not crash
     */
    EXPECT_NO_CRASH(nimcp_cleanup_clear(nullptr));
}

TEST_F(NullSafetyRegressionTest, CleanupStack_PopWithNull) {
    /**
     * REGRESSION TEST: nimcp_cleanup_pop(NULL) should not crash
     */
    nimcp_cleanup_entry_t entry;
    EXPECT_NO_CRASH(entry = nimcp_cleanup_pop(nullptr));
    EXPECT_EQ(entry.cleanup, nullptr);
    EXPECT_EQ(entry.resource, nullptr);
}

//=============================================================================
// DOUBLE-NULL AND EDGE CASES
//=============================================================================

TEST_F(NullSafetyRegressionTest, DoubleNull_AllParametersNull) {
    /**
     * REGRESSION TEST: Functions should handle ALL parameters being NULL
     */
    bool result = true;
    int iresult = 0;

    /* Motor */
    EXPECT_NO_CRASH(result = motor_get_config(nullptr, nullptr));
    EXPECT_FALSE(result);

    EXPECT_NO_CRASH(result = motor_get_effector_state(nullptr, 0, nullptr));
    EXPECT_FALSE(result);

    EXPECT_NO_CRASH(result = motor_update_feedback(nullptr, 0, nullptr));
    EXPECT_FALSE(result);

    /* SNN */
    EXPECT_NO_CRASH(iresult = snn_network_set_inputs(nullptr, nullptr, 0));
    EXPECT_NE(iresult, 0);

    EXPECT_NO_CRASH(iresult = snn_network_get_outputs(nullptr, nullptr, 0));
    EXPECT_NE(iresult, 0);
}

TEST_F(NullSafetyRegressionTest, EmptyArrays_WithNull) {
    /**
     * REGRESSION TEST: Zero-length arrays with NULL should not crash
     */
    motor_adapter_t* adapter = motor_create(nullptr);
    if (!adapter) {
        GTEST_SKIP() << "Could not create motor adapter";
    }

    bool result = true;
    EXPECT_NO_CRASH(result = motor_plan_trajectory(adapter, MOTOR_REGION_HAND_LEFT, nullptr, 0));
    /* Result may be false (no waypoints) but should not crash */

    motor_destroy(adapter);
}

//=============================================================================
// SYSTEMATIC API COVERAGE
//=============================================================================

TEST_F(NullSafetyRegressionTest, Systematic_MotorProgramFunctions) {
    /**
     * REGRESSION TEST: Motor program functions should handle NULL safely
     */
    motor_adapter_t* adapter = motor_create(nullptr);
    if (!adapter) {
        GTEST_SKIP() << "Could not create motor adapter";
    }

    uint32_t program_id = 0;
    motor_program_info_t info = {};
    motor_command_t cmd = {};
    bool result = true;

    /* Store program with NULL params */
    EXPECT_NO_CRASH(program_id = motor_store_program(nullptr, nullptr, nullptr, 0, MOVEMENT_TYPE_DISCRETE));
    EXPECT_EQ(program_id, 0u);

    EXPECT_NO_CRASH(program_id = motor_store_program(adapter, nullptr, nullptr, 0, MOVEMENT_TYPE_DISCRETE));
    /* May succeed or fail, but should not crash */

    EXPECT_NO_CRASH(program_id = motor_store_program(adapter, "test", nullptr, 0, MOVEMENT_TYPE_DISCRETE));
    /* Should handle NULL commands gracefully */

    /* Get program with NULL params */
    EXPECT_NO_CRASH(result = motor_get_program(nullptr, 1, nullptr));
    EXPECT_FALSE(result);

    EXPECT_NO_CRASH(result = motor_get_program(adapter, 1, nullptr));
    EXPECT_FALSE(result);

    EXPECT_NO_CRASH(result = motor_get_program(nullptr, 1, &info));
    EXPECT_FALSE(result);

    /* Delete program with NULL */
    EXPECT_NO_CRASH(result = motor_delete_program(nullptr, 1));
    EXPECT_FALSE(result);

    motor_destroy(adapter);
}

TEST_F(NullSafetyRegressionTest, Systematic_MotorExecutionFunctions) {
    /**
     * REGRESSION TEST: Motor execution functions should handle NULL safely
     */
    bool result = true;
    motor_command_t cmd = {};
    motor_result_t motor_result = {};

    /* Begin execution */
    EXPECT_NO_CRASH(result = motor_begin_execution(nullptr));
    EXPECT_FALSE(result);

    /* Update execution */
    EXPECT_NO_CRASH(result = motor_update_execution(nullptr, 10.0f));
    EXPECT_FALSE(result);

    /* Stop execution */
    EXPECT_NO_CRASH(result = motor_stop_execution(nullptr));
    EXPECT_FALSE(result);

    /* Get next command */
    EXPECT_NO_CRASH(result = motor_get_next_command(nullptr, &cmd));
    EXPECT_FALSE(result);

    /* Get result */
    EXPECT_NO_CRASH(result = motor_get_result(nullptr, &motor_result));
    EXPECT_FALSE(result);

    motor_adapter_t* adapter = motor_create(nullptr);
    if (adapter) {
        EXPECT_NO_CRASH(result = motor_get_next_command(adapter, nullptr));
        EXPECT_FALSE(result);

        EXPECT_NO_CRASH(result = motor_get_result(adapter, nullptr));
        EXPECT_FALSE(result);

        motor_destroy(adapter);
    }
}

TEST_F(NullSafetyRegressionTest, Systematic_MotorIntegrationFunctions) {
    /**
     * REGRESSION TEST: Motor integration functions should handle NULL safely
     */
    bool result = true;
    motor_vec3_t pos = {0, 0, 0};

    /* Basal ganglia selection */
    EXPECT_NO_CRASH(result = motor_receive_bg_selection(nullptr, 1, 0.5f));
    EXPECT_FALSE(result);

    /* Cerebellar correction */
    EXPECT_NO_CRASH(result = motor_receive_cerebellar_correction(nullptr, 1, 0.0f, 1.0f));
    EXPECT_FALSE(result);

    /* Visual feedback */
    EXPECT_NO_CRASH(result = motor_update_visual_feedback(nullptr, 1, nullptr, 0.5f));
    EXPECT_FALSE(result);

    motor_adapter_t* adapter = motor_create(nullptr);
    if (adapter) {
        EXPECT_NO_CRASH(result = motor_update_visual_feedback(adapter, 1, nullptr, 0.5f));
        EXPECT_FALSE(result);

        motor_destroy(adapter);
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
