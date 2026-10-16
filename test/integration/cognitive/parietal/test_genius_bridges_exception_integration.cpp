/**
 * @file test_genius_bridges_exception_integration.cpp
 * @brief Integration tests for exception handling across Mathematical Genius bridges
 * @date 2026-01-24
 *
 * WHAT: Test exception handling integration between genius bridges and immune system
 * WHY:  Verify exceptions propagate correctly through multi-bridge workflows
 * HOW:  Create multi-bridge scenarios and verify exception handling
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>

extern "C" {
#include "cognitive/parietal/nimcp_genius_snn_bridge.h"
#include "cognitive/parietal/nimcp_genius_plasticity_bridge.h"
#include "cognitive/parietal/nimcp_genius_training_bridge.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class GeniusBridgesExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<int> last_exception_code;
    static nimcp_handler_registration_t* registration;

    genius_snn_bridge_t* snn_bridge;
    genius_plasticity_bridge_t* plasticity_bridge;
    genius_training_bridge_t* training_bridge;

    void SetUp() override {
        exception_count = 0;
        last_exception_code = 0;

        nimcp_exception_system_init();

        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.name = "integration_test_handler";
        registration = nimcp_handler_register(&options);

        // Create all bridges
        genius_snn_config_t snn_config = genius_snn_config_default();
        snn_bridge = genius_snn_create(&snn_config);

        genius_plasticity_config_t plasticity_config = genius_plasticity_config_default();
        plasticity_bridge = genius_plasticity_create(&plasticity_config);

        genius_training_config_t training_config = genius_training_config_default();
        training_bridge = genius_training_create(&training_config);
    }

    void TearDown() override {
        if (snn_bridge) genius_snn_destroy(snn_bridge);
        if (plasticity_bridge) genius_plasticity_destroy(plasticity_bridge);
        if (training_bridge) genius_training_destroy(training_bridge);

        if (registration) {
            nimcp_handler_unregister(registration);
            registration = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_count++;
        last_exception_code = ex->code;
        return false;
    }
};

std::atomic<int> GeniusBridgesExceptionIntegrationTest::exception_count{0};
std::atomic<int> GeniusBridgesExceptionIntegrationTest::last_exception_code{0};
nimcp_handler_registration_t* GeniusBridgesExceptionIntegrationTest::registration = nullptr;

//=============================================================================
// Multi-Bridge Exception Propagation Tests
//=============================================================================

TEST_F(GeniusBridgesExceptionIntegrationTest, AllBridges_CreateSuccessfully_NoExceptions) {
    ASSERT_NE(snn_bridge, nullptr);
    ASSERT_NE(plasticity_bridge, nullptr);
    ASSERT_NE(training_bridge, nullptr);
    EXPECT_EQ(exception_count.load(), 0);
}

TEST_F(GeniusBridgesExceptionIntegrationTest, SNNThenPlasticity_ExceptionInSNN_PlasticityUnaffected) {
    exception_count = 0;

    // First operation: SNN with invalid input (triggers exception)
    int snn_result = genius_snn_encode_state(snn_bridge, nullptr, 8);
    EXPECT_LT(snn_result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);

    int prev_count = exception_count.load();

    // Second operation: Plasticity with valid input (should work)
    // Register a synapse first
    int reg_result = genius_plasticity_register_synapse(
        plasticity_bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);
    EXPECT_EQ(reg_result, 0);

    // Learn on that synapse
    int learn_result = genius_plasticity_learn(
        plasticity_bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 1, 1.0f);
    EXPECT_EQ(learn_result, 0);

    // No new exceptions from plasticity
    EXPECT_EQ(exception_count.load(), prev_count);
}

TEST_F(GeniusBridgesExceptionIntegrationTest, PlasticityThenTraining_ExceptionInPlasticity_TrainingUnaffected) {
    exception_count = 0;

    // First operation: Plasticity with invalid synapse (triggers exception)
    int plasticity_result = genius_plasticity_learn(
        plasticity_bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 99999, 1.0f);
    EXPECT_LT(plasticity_result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NOT_FOUND);

    int prev_count = exception_count.load();

    // Second operation: Training with valid input (should work)
    float inputs[32] = {0.5f};
    float targets[32] = {0.5f};
    float loss = genius_training_train_batch(training_bridge, inputs, targets, 32);
    EXPECT_GE(loss, 0.0f);

    // No new exceptions from training
    EXPECT_EQ(exception_count.load(), prev_count);
}

TEST_F(GeniusBridgesExceptionIntegrationTest, SequentialExceptions_AllTracked_CountMatches) {
    exception_count = 0;

    // Trigger exception in SNN
    genius_snn_encode_state(nullptr, nullptr, 0);
    EXPECT_EQ(exception_count.load(), 1);

    // Trigger exception in Plasticity
    genius_plasticity_learn(nullptr, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 1, 1.0f);
    EXPECT_EQ(exception_count.load(), 2);

    // Trigger exception in Training
    genius_training_train_batch(nullptr, nullptr, nullptr, 0);
    EXPECT_EQ(exception_count.load(), 3);
}

//=============================================================================
// Cross-Bridge Workflow Tests with Exception Handling
//=============================================================================

TEST_F(GeniusBridgesExceptionIntegrationTest, SNNPlasticityWorkflow_ValidFlow_NoExceptions) {
    exception_count = 0;

    // Step 1: Encode mathematical state via SNN
    float dimensions[8] = {0.8f, 0.6f, 0.7f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f};
    int spikes = genius_snn_encode_state(snn_bridge, dimensions, 8);
    EXPECT_GE(spikes, 0);

    // Step 2: Register synapse for learning
    int reg = genius_plasticity_register_synapse(
        plasticity_bridge, 100, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_NEWTON);
    EXPECT_EQ(reg, 0);

    // Step 3: Apply learning based on SNN activity
    int learn = genius_plasticity_learn(
        plasticity_bridge, GENIUS_LEARN_PATTERN_FOUND, 0.7f, 100, 1.0f);
    EXPECT_EQ(learn, 0);

    // No exceptions in valid workflow
    EXPECT_EQ(exception_count.load(), 0);
}

TEST_F(GeniusBridgesExceptionIntegrationTest, TrainingPlasticityWorkflow_ValidFlow_NoExceptions) {
    exception_count = 0;

    // Step 1: Train a batch
    float inputs[32] = {0.5f};
    float targets[32] = {0.5f};
    float loss = genius_training_train_batch(training_bridge, inputs, targets, 32);
    EXPECT_GE(loss, 0.0f);

    // Step 2: Register synapse
    int reg = genius_plasticity_register_synapse(
        plasticity_bridge, 200, GENIUS_SYNAPSE_ELEGANCE, 0.3f, GENIUS_MODE_ERDOS);
    EXPECT_EQ(reg, 0);

    // Step 3: Apply insight reward
    int insight = genius_plasticity_apply_insight_reward(plasticity_bridge, 0.8f, GENIUS_MODE_ERDOS);
    EXPECT_EQ(insight, 0);

    // No exceptions
    EXPECT_EQ(exception_count.load(), 0);
}

//=============================================================================
// Concurrent Exception Handling Tests
//=============================================================================

TEST_F(GeniusBridgesExceptionIntegrationTest, ConcurrentExceptions_AllHandled_NoRaceConditions) {
    exception_count = 0;

    std::thread t1([this]() {
        for (int i = 0; i < 10; i++) {
            genius_snn_encode_state(nullptr, nullptr, 0);
        }
    });

    std::thread t2([this]() {
        for (int i = 0; i < 10; i++) {
            genius_plasticity_learn(nullptr, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 1, 1.0f);
        }
    });

    std::thread t3([this]() {
        for (int i = 0; i < 10; i++) {
            genius_training_train_batch(nullptr, nullptr, nullptr, 0);
        }
    });

    t1.join();
    t2.join();
    t3.join();

    // All 30 exceptions should have been handled
    EXPECT_EQ(exception_count.load(), 30);
}

//=============================================================================
// Exception Recovery Tests
//=============================================================================

TEST_F(GeniusBridgesExceptionIntegrationTest, AfterException_BridgeStillFunctional_CanContinue) {
    exception_count = 0;

    // Trigger an exception
    int bad_result = genius_snn_encode_state(snn_bridge, nullptr, 8);
    EXPECT_LT(bad_result, 0);
    EXPECT_GT(exception_count.load(), 0);

    // Bridge should still be functional
    float dimensions[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    int good_result = genius_snn_encode_state(snn_bridge, dimensions, 8);
    EXPECT_GE(good_result, 0);
}

TEST_F(GeniusBridgesExceptionIntegrationTest, MultipleExceptionTypes_AllDistinguishable_CodesCorrect) {
    // Test NULL_POINTER
    exception_count = 0;
    genius_snn_encode_state(nullptr, nullptr, 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);

    // Test NOT_FOUND
    genius_plasticity_learn(plasticity_bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 99999, 1.0f);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NOT_FOUND);

    // Test INVALID_PARAM
    genius_plasticity_get_mode_skill(plasticity_bridge, (genius_mode_t)999);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_INVALID_PARAM);
}
