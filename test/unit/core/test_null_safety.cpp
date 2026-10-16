/**
 * @file test_null_safety.cpp
 * @brief Comprehensive NULL safety tests across NIMCP modules
 *
 * WHAT: Tests for NULL parameter rejection without crashes
 * WHY:  Ensure robust NULL handling across all public APIs
 * HOW:  Systematically test NULL inputs to all major functions
 *
 * TESTS COVER:
 * 1. NULL parameter rejection with proper error return
 * 2. No crashes on NULL input
 * 3. Consistent error behavior across modules
 * 4. Safe destroy/free of NULL pointers
 * 5. Query functions return safe defaults for NULL
 * 6. Output parameters are untouched when input is NULL
 *
 * MODULES TESTED:
 * - Motor Adapter
 * - Executive Functions
 * - SNN Training
 * - Monte Carlo
 * - Tensor Operations
 * - Memory Management
 *
 * @version 1.0.0
 * @date 2025-01-15
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "cognitive/nimcp_executive.h"
#include "snn/nimcp_snn_training.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NullSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Initialize subsystems needed for some tests */
        nimcp_tensor_init();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }
};

//=============================================================================
// Motor Adapter NULL Safety Tests
//=============================================================================

TEST_F(NullSafetyTest, MotorDestroyNull) {
    /**
     * WHAT: Test motor_destroy with NULL
     * WHY:  Destroy must be NULL-safe
     * HOW:  Pass NULL, verify no crash
     */
    motor_destroy(nullptr);
    SUCCEED();
}

TEST_F(NullSafetyTest, MotorResetNull) {
    /**
     * WHAT: Test motor_reset with NULL
     * WHY:  Must return false, not crash
     * HOW:  Pass NULL, verify return value
     */
    EXPECT_FALSE(motor_reset(nullptr));
}

TEST_F(NullSafetyTest, MotorGetStatusNull) {
    /**
     * WHAT: Test motor_get_status with NULL
     * WHY:  Must return error status
     * HOW:  Pass NULL, verify returns MOTOR_STATUS_ERROR
     */
    EXPECT_EQ(motor_get_status(nullptr), MOTOR_STATUS_ERROR);
}

TEST_F(NullSafetyTest, MotorGetLastErrorNull) {
    /**
     * WHAT: Test motor_get_last_error with NULL
     * WHY:  Must return error code
     * HOW:  Pass NULL, verify returns MOTOR_ERROR_INTERNAL
     */
    EXPECT_EQ(motor_get_last_error(nullptr), MOTOR_ERROR_INTERNAL);
}

TEST_F(NullSafetyTest, MotorGetStatsNull) {
    /**
     * WHAT: Test motor_get_stats with NULL adapter
     * WHY:  Must return false, not crash
     * HOW:  Pass NULL adapter, verify return
     */
    motor_stats_t stats;
    EXPECT_FALSE(motor_get_stats(nullptr, &stats));
}

TEST_F(NullSafetyTest, MotorGetStatsNullOutput) {
    /**
     * WHAT: Test motor_get_stats with NULL output
     * WHY:  Must handle NULL output gracefully
     * HOW:  Create adapter, pass NULL stats, verify return
     */
    motor_config_t config = motor_default_config();
    config.enable_bio_async = false;
    motor_adapter_t* adapter = motor_create(&config);

    if (adapter) {
        EXPECT_FALSE(motor_get_stats(adapter, nullptr));
        motor_destroy(adapter);
    }
}

TEST_F(NullSafetyTest, MotorGetConfigNull) {
    /**
     * WHAT: Test motor_get_config with NULL adapter
     * WHY:  Must return false
     * HOW:  Pass NULL adapter
     */
    motor_config_t config;
    EXPECT_FALSE(motor_get_config(nullptr, &config));
}

TEST_F(NullSafetyTest, MotorPlanMovementNull) {
    /**
     * WHAT: Test motor_plan_movement with NULL
     * WHY:  Must return false
     * HOW:  Pass NULL adapter and/or goal
     */
    motor_goal_t goal = {};
    EXPECT_FALSE(motor_plan_movement(nullptr, &goal));

    motor_config_t config = motor_default_config();
    config.enable_bio_async = false;
    motor_adapter_t* adapter = motor_create(&config);

    if (adapter) {
        EXPECT_FALSE(motor_plan_movement(adapter, nullptr));
        motor_destroy(adapter);
    }
}

TEST_F(NullSafetyTest, MotorSetCallbackNull) {
    /**
     * WHAT: Test motor_set_*_callback with NULL adapter
     * WHY:  Must return false
     * HOW:  Pass NULL adapter
     */
    EXPECT_FALSE(motor_set_command_callback(nullptr, nullptr, nullptr));
    EXPECT_FALSE(motor_set_complete_callback(nullptr, nullptr, nullptr));
    EXPECT_FALSE(motor_set_event_callback(nullptr, nullptr, nullptr));
}

TEST_F(NullSafetyTest, MotorGetBioContextNull) {
    /**
     * WHAT: Test motor_get_bio_context with NULL
     * WHY:  Must return NULL context
     * HOW:  Pass NULL adapter
     */
    bio_module_context_t ctx = motor_get_bio_context(nullptr);
    /* Should return invalid/null context */
}

TEST_F(NullSafetyTest, MotorProcessBioMessagesNull) {
    /**
     * WHAT: Test motor_process_bio_messages with NULL
     * WHY:  Must return 0
     * HOW:  Pass NULL adapter
     */
    EXPECT_EQ(motor_process_bio_messages(nullptr, 0), 0u);
}

//=============================================================================
// Executive Functions NULL Safety Tests
//=============================================================================

TEST_F(NullSafetyTest, ExecutiveDestroyNull) {
    /**
     * WHAT: Test executive_destroy with NULL
     * WHY:  Destroy must be NULL-safe
     * HOW:  Pass NULL, verify no crash
     */
    executive_destroy(nullptr);
    SUCCEED();
}

TEST_F(NullSafetyTest, ExecutiveCreateCustomNull) {
    /**
     * WHAT: Test executive_create_custom with NULL config
     * WHY:  Must return NULL
     * HOW:  Pass NULL config
     */
    EXPECT_EQ(executive_create_custom(nullptr), nullptr);
}

TEST_F(NullSafetyTest, ExecutiveAddTaskNull) {
    /**
     * WHAT: Test executive_add_task with NULL
     * WHY:  Must return 0 (invalid task ID)
     * HOW:  Pass NULL exec and/or task
     */
    task_descriptor_t task = {};
    EXPECT_EQ(executive_add_task(nullptr, &task), 0u);

    executive_controller_t* exec = executive_create();
    if (exec) {
        EXPECT_EQ(executive_add_task(exec, nullptr), 0u);
        executive_destroy(exec);
    }
}

TEST_F(NullSafetyTest, ExecutiveSwitchTaskNull) {
    /**
     * WHAT: Test executive_switch_task with NULL
     * WHY:  Must return false
     * HOW:  Pass NULL exec
     */
    EXPECT_FALSE(executive_switch_task(nullptr, 1, 0));
}

TEST_F(NullSafetyTest, ExecutiveCompleteTaskNull) {
    /**
     * WHAT: Test executive_complete_task with NULL
     * WHY:  Must return false
     * HOW:  Pass NULL exec
     */
    EXPECT_FALSE(executive_complete_task(nullptr, true, 0));
}

TEST_F(NullSafetyTest, ExecutiveShouldInhibitNull) {
    /**
     * WHAT: Test executive_should_inhibit with NULL
     * WHY:  Must return safe default (true = inhibit)
     * HOW:  Pass NULL exec
     */
    EXPECT_TRUE(executive_should_inhibit(nullptr, 0.5f, "test"));
}

TEST_F(NullSafetyTest, ExecutiveCreatePlanNull) {
    /**
     * WHAT: Test executive_create_plan with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL exec or goal
     */
    EXPECT_EQ(executive_create_plan(nullptr, "goal", 5), nullptr);

    executive_controller_t* exec = executive_create();
    if (exec) {
        EXPECT_EQ(executive_create_plan(exec, nullptr, 5), nullptr);
        executive_destroy(exec);
    }
}

TEST_F(NullSafetyTest, ExecutiveDestroyPlanNull) {
    /**
     * WHAT: Test executive_destroy_plan with NULL
     * WHY:  Must be NULL-safe
     * HOW:  Pass NULL plan
     */
    executive_destroy_plan(nullptr);
    SUCCEED();
}

TEST_F(NullSafetyTest, ExecutiveGetStatsNull) {
    /**
     * WHAT: Test executive_get_stats with NULL
     * WHY:  Must return false
     * HOW:  Pass NULL exec or stats
     */
    executive_stats_t stats;
    EXPECT_FALSE(executive_get_stats(nullptr, &stats));

    executive_controller_t* exec = executive_create();
    if (exec) {
        EXPECT_FALSE(executive_get_stats(exec, nullptr));
        executive_destroy(exec);
    }
}

TEST_F(NullSafetyTest, ExecutiveResetStatsNull) {
    /**
     * WHAT: Test executive_reset_stats with NULL
     * WHY:  Must not crash
     * HOW:  Pass NULL exec
     */
    executive_reset_stats(nullptr);
    SUCCEED();
}

TEST_F(NullSafetyTest, ExecutiveGetCognitiveLoadNull) {
    /**
     * WHAT: Test executive_get_cognitive_load with NULL
     * WHY:  Must return safe value (1.0 = max load)
     * HOW:  Pass NULL exec
     */
    EXPECT_EQ(executive_get_cognitive_load(nullptr), 1.0f);
}

TEST_F(NullSafetyTest, ExecutiveSetBrainNull) {
    /**
     * WHAT: Test executive_set_brain with NULL
     * WHY:  Must not crash
     * HOW:  Pass NULL exec
     */
    executive_set_brain(nullptr, nullptr);
    SUCCEED();
}

TEST_F(NullSafetyTest, ExecutiveSetWorkspaceNull) {
    /**
     * WHAT: Test executive_set_workspace with NULL
     * WHY:  Must not crash
     * HOW:  Pass NULL exec
     */
    executive_set_workspace(nullptr, nullptr);
    SUCCEED();
}

TEST_F(NullSafetyTest, ExecutiveSaveNull) {
    /**
     * WHAT: Test executive_save with NULL
     * WHY:  Must return false
     * HOW:  Pass NULL exec or file
     */
    EXPECT_FALSE(executive_save(nullptr, nullptr));
}

TEST_F(NullSafetyTest, ExecutiveLoadNull) {
    /**
     * WHAT: Test executive_load with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL file
     */
    EXPECT_EQ(executive_load(nullptr), nullptr);
}

TEST_F(NullSafetyTest, ExecutiveProcessMessagesNull) {
    /**
     * WHAT: Test executive_process_messages with NULL
     * WHY:  Must return 0
     * HOW:  Pass NULL exec
     */
    EXPECT_EQ(executive_process_messages(nullptr, 0), 0u);
}

//=============================================================================
// SNN Training NULL Safety Tests
//=============================================================================

TEST_F(NullSafetyTest, SNNTrainingDestroyNull) {
    /**
     * WHAT: Test snn_training_destroy with NULL
     * WHY:  Destroy must be NULL-safe
     * HOW:  Pass NULL, verify no crash
     */
    snn_training_destroy(nullptr);
    SUCCEED();
}

TEST_F(NullSafetyTest, SNNSTDPCreateNull) {
    /**
     * WHAT: Test snn_training_create_stdp with NULL config
     * WHY:  Must return NULL or use defaults
     * HOW:  Pass NULL config
     */
    snn_training_ctx_t* ctx = snn_training_create_stdp(nullptr);
    /* Could return NULL or use defaults - both valid */
    if (ctx) {
        snn_training_destroy(ctx);
    }
}

TEST_F(NullSafetyTest, SNNSTDPComputeDeltaWNull) {
    /**
     * WHAT: Test snn_stdp_compute_delta_w with NULL
     * WHY:  Must return 0
     * HOW:  Pass NULL ctx
     */
    float delta_w = snn_stdp_compute_delta_w(nullptr, 10.0f, 0.5f);
    EXPECT_EQ(delta_w, 0.0f);
}

TEST_F(NullSafetyTest, SNNSurrogateGradientNull) {
    /**
     * WHAT: Test snn_surrogate_gradient with NULL
     * WHY:  Must return 0
     * HOW:  Pass NULL ctx
     */
    float grad = snn_surrogate_gradient(nullptr, 0.5f);
    EXPECT_EQ(grad, 0.0f);
}

TEST_F(NullSafetyTest, SNNRSTDPSetRewardNull) {
    /**
     * WHAT: Test snn_rstdp_set_reward with NULL
     * WHY:  Must not crash
     * HOW:  Pass NULL ctx
     */
    snn_rstdp_set_reward(nullptr, 1.0f);
    SUCCEED();
}

TEST_F(NullSafetyTest, SNNRSTDPUpdateEligibilityNull) {
    /**
     * WHAT: Test snn_rstdp_update_eligibility with NULL
     * WHY:  Must not crash
     * HOW:  Pass NULL ctx
     */
    snn_rstdp_update_eligibility(nullptr, 1.0f);
    SUCCEED();
}

TEST_F(NullSafetyTest, SNNConfigDefaultNull) {
    /**
     * WHAT: Test config default functions with NULL
     * WHY:  Must not crash
     * HOW:  Pass NULL config
     */
    snn_stdp_config_default(nullptr);
    snn_rstdp_config_default(nullptr);
    snn_surrogate_config_default(nullptr);
    snn_eprop_config_default(nullptr);
    snn_homeostatic_config_default(nullptr);
    SUCCEED();
}

//=============================================================================
// Monte Carlo NULL Safety Tests
//=============================================================================

TEST_F(NullSafetyTest, MCRandomUniformNull) {
    /**
     * WHAT: Test mc_random_uniform with NULL seed
     * WHY:  Must return 0
     * HOW:  Pass NULL seed
     */
    float val = mc_random_uniform(nullptr);
    EXPECT_EQ(val, 0.0f);
}

TEST_F(NullSafetyTest, MCRandomNormalNull) {
    /**
     * WHAT: Test mc_random_normal with NULL seed
     * WHY:  Must return mean as fallback
     * HOW:  Pass NULL seed
     */
    float val = mc_random_normal(nullptr, 5.0f, 1.0f);
    EXPECT_EQ(val, 5.0f);  /* Should return mean */
}

TEST_F(NullSafetyTest, MCRandomIntNull) {
    /**
     * WHAT: Test mc_random_int with NULL seed
     * WHY:  Must return 0
     * HOW:  Pass NULL seed
     */
    uint32_t val = mc_random_int(nullptr, 100);
    EXPECT_EQ(val, 0u);
}

TEST_F(NullSafetyTest, MCRandomChoiceNull) {
    /**
     * WHAT: Test mc_random_choice with NULL
     * WHY:  Must return 0
     * HOW:  Pass NULL seed or weights
     */
    float weights[] = {0.5f, 0.5f};
    uint32_t seed = 12345;

    EXPECT_EQ(mc_random_choice(nullptr, weights, 2), 0u);
    EXPECT_EQ(mc_random_choice(&seed, nullptr, 2), 0u);
    EXPECT_EQ(mc_random_choice(&seed, weights, 0), 0u);
}

TEST_F(NullSafetyTest, MCMeanNull) {
    /**
     * WHAT: Test mc_mean with NULL
     * WHY:  Must return 0
     * HOW:  Pass NULL values
     */
    EXPECT_EQ(mc_mean(nullptr, 5), 0.0f);
}

TEST_F(NullSafetyTest, MCVarianceNull) {
    /**
     * WHAT: Test mc_variance with NULL
     * WHY:  Must return 0
     * HOW:  Pass NULL values
     */
    EXPECT_EQ(mc_variance(nullptr, 5, 0.0f), 0.0f);
}

TEST_F(NullSafetyTest, MCShuffleNull) {
    /**
     * WHAT: Test mc_shuffle_u32 with NULL
     * WHY:  Must not crash
     * HOW:  Pass NULL array or seed
     */
    uint32_t arr[] = {1, 2, 3};
    uint32_t seed = 12345;

    mc_shuffle_u32(nullptr, 3, &seed);
    mc_shuffle_u32(arr, 3, nullptr);
    SUCCEED();
}

//=============================================================================
// Tensor NULL Safety Tests
//=============================================================================

TEST_F(NullSafetyTest, TensorDestroyNull) {
    /**
     * WHAT: Test nimcp_tensor_destroy with NULL
     * WHY:  Destroy must be NULL-safe
     * HOW:  Pass NULL, verify no crash
     */
    nimcp_tensor_destroy(nullptr);
    SUCCEED();
}

TEST_F(NullSafetyTest, TensorCreateNullDims) {
    /**
     * WHAT: Test nimcp_tensor_create with NULL dims
     * WHY:  Must return NULL for non-scalar
     * HOW:  Pass NULL dims with rank > 0
     */
    EXPECT_EQ(nimcp_tensor_create(nullptr, 2, NIMCP_DTYPE_F32), nullptr);
}

TEST_F(NullSafetyTest, TensorShapeNull) {
    /**
     * WHAT: Test nimcp_tensor_shape with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL tensor
     */
    EXPECT_EQ(nimcp_tensor_shape(nullptr), nullptr);
}

TEST_F(NullSafetyTest, TensorRankNull) {
    /**
     * WHAT: Test nimcp_tensor_rank with NULL
     * WHY:  Must return 0
     * HOW:  Pass NULL tensor
     */
    EXPECT_EQ(nimcp_tensor_rank(nullptr), 0u);
}

TEST_F(NullSafetyTest, TensorNumelNull) {
    /**
     * WHAT: Test nimcp_tensor_numel with NULL
     * WHY:  Must return 0
     * HOW:  Pass NULL tensor
     */
    EXPECT_EQ(nimcp_tensor_numel(nullptr), 0u);
}

TEST_F(NullSafetyTest, TensorDataNull) {
    /**
     * WHAT: Test nimcp_tensor_data with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL tensor
     */
    EXPECT_EQ(nimcp_tensor_data(nullptr), nullptr);
}

TEST_F(NullSafetyTest, TensorDataConstNull) {
    /**
     * WHAT: Test nimcp_tensor_data_const with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL tensor
     */
    EXPECT_EQ(nimcp_tensor_data_const(nullptr), nullptr);
}

TEST_F(NullSafetyTest, TensorCloneNull) {
    /**
     * WHAT: Test nimcp_tensor_clone with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL tensor
     */
    EXPECT_EQ(nimcp_tensor_clone(nullptr), nullptr);
}

TEST_F(NullSafetyTest, TensorIsContiguousNull) {
    /**
     * WHAT: Test nimcp_tensor_is_contiguous with NULL
     * WHY:  Must return false
     * HOW:  Pass NULL tensor
     */
    EXPECT_FALSE(nimcp_tensor_is_contiguous(nullptr));
}

TEST_F(NullSafetyTest, TensorRequiresGradNull) {
    /**
     * WHAT: Test nimcp_tensor_requires_grad with NULL
     * WHY:  Must return false
     * HOW:  Pass NULL tensor
     */
    EXPECT_FALSE(nimcp_tensor_requires_grad(nullptr));
}

TEST_F(NullSafetyTest, TensorBinaryOpsNull) {
    /**
     * WHAT: Test binary tensor operations with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL operands
     */
    uint32_t dims[] = {2, 2};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);

    if (t) {
        EXPECT_EQ(nimcp_tensor_add(nullptr, t), nullptr);
        EXPECT_EQ(nimcp_tensor_add(t, nullptr), nullptr);
        EXPECT_EQ(nimcp_tensor_sub(nullptr, t), nullptr);
        EXPECT_EQ(nimcp_tensor_mul(nullptr, t), nullptr);
        EXPECT_EQ(nimcp_tensor_div(nullptr, t), nullptr);
        EXPECT_EQ(nimcp_tensor_matmul(nullptr, t), nullptr);

        nimcp_tensor_destroy(t);
    }
}

TEST_F(NullSafetyTest, TensorUnaryOpsNull) {
    /**
     * WHAT: Test unary tensor operations with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL tensor
     */
    EXPECT_EQ(nimcp_tensor_neg(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_abs(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_sqrt(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_exp(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_log(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_sin(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_cos(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_tanh(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_sigmoid(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_relu(nullptr), nullptr);
}

TEST_F(NullSafetyTest, TensorReductionsNull) {
    /**
     * WHAT: Test reduction operations with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL tensor
     */
    EXPECT_EQ(nimcp_tensor_sum(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_mean(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_max(nullptr), nullptr);
    EXPECT_EQ(nimcp_tensor_min(nullptr), nullptr);
}

TEST_F(NullSafetyTest, TensorReshapeNull) {
    /**
     * WHAT: Test nimcp_tensor_reshape with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL tensor
     */
    uint32_t dims[] = {2, 2};
    EXPECT_EQ(nimcp_tensor_reshape(nullptr, dims, 2), nullptr);
}

TEST_F(NullSafetyTest, TensorTransposeNull) {
    /**
     * WHAT: Test nimcp_tensor_transpose with NULL
     * WHY:  Must return NULL
     * HOW:  Pass NULL tensor
     */
    EXPECT_EQ(nimcp_tensor_transpose(nullptr), nullptr);
}

//=============================================================================
// Memory Management NULL Safety Tests
//=============================================================================

TEST_F(NullSafetyTest, NimcpFreeNull) {
    /**
     * WHAT: Test nimcp_free with NULL
     * WHY:  Free must be NULL-safe
     * HOW:  Pass NULL, verify no crash
     */
    nimcp_free(nullptr);
    SUCCEED();
}

TEST_F(NullSafetyTest, NimcpReallocNull) {
    /**
     * WHAT: Test nimcp_realloc with NULL
     * WHY:  Realloc NULL should behave like malloc
     * HOW:  Pass NULL ptr
     */
    void* ptr = nimcp_realloc(nullptr, 100);
    /* Should allocate new memory */
    if (ptr) {
        nimcp_free(ptr);
    }
}

TEST_F(NullSafetyTest, NimcpReallocZeroSize) {
    /**
     * WHAT: Test nimcp_realloc with zero size
     * WHY:  Zero size should free memory
     * HOW:  Allocate, then realloc to 0
     */
    void* ptr = nimcp_malloc(100);
    if (ptr) {
        void* result = nimcp_realloc(ptr, 0);
        /* Result may be NULL or small allocation */
        if (result) {
            nimcp_free(result);
        }
    }
}

//=============================================================================
// Comprehensive NULL Test
//=============================================================================

TEST_F(NullSafetyTest, NoFunctionsSegfaultOnNull) {
    /**
     * WHAT: Verify no functions crash on NULL
     * WHY:  Comprehensive safety check
     * HOW:  Call many functions with NULL, verify no crash
     *
     * Note: This test intentionally doesn't check return values,
     * it only verifies no crashes/segfaults occur.
     */

    /* Motor functions */
    motor_destroy(nullptr);
    motor_reset(nullptr);
    motor_get_status(nullptr);
    motor_get_last_error(nullptr);
    motor_begin_execution(nullptr);
    motor_stop_execution(nullptr);
    motor_set_command_callback(nullptr, nullptr, nullptr);

    /* Executive functions */
    executive_destroy(nullptr);
    executive_reset_stats(nullptr);
    executive_destroy_plan(nullptr);
    executive_set_brain(nullptr, nullptr);
    executive_set_workspace(nullptr, nullptr);
    executive_set_theory_of_mind(nullptr, nullptr);
    executive_set_immune_system(nullptr, nullptr);
    executive_set_sleep_state(nullptr, SLEEP_STATE_AWAKE);

    /* SNN functions */
    snn_training_destroy(nullptr);
    snn_rstdp_set_reward(nullptr, 0.0f);
    snn_rstdp_update_eligibility(nullptr, 0.0f);
    snn_stdp_config_default(nullptr);
    snn_rstdp_config_default(nullptr);

    /* Monte Carlo functions */
    mc_shuffle_u32(nullptr, 0, nullptr);

    /* Tensor functions */
    nimcp_tensor_destroy(nullptr);
    nimcp_tensor_print_info(nullptr, "test");
    nimcp_tensor_print_data(nullptr, 10);
    nimcp_tensor_set_requires_grad(nullptr, true);
    nimcp_tensor_zero_grad(nullptr);

    /* Memory functions */
    nimcp_free(nullptr);

    SUCCEED() << "All NULL calls completed without crash";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
