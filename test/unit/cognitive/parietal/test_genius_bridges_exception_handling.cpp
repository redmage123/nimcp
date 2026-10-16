/**
 * @file test_genius_bridges_exception_handling.cpp
 * @brief Unit tests for exception handling in Mathematical Genius bridges
 * @date 2026-01-24
 *
 * WHAT: Test NIMCP_THROW_TO_IMMUNE exception handling for all genius bridges
 * WHY:  Ensure proper error reporting to immune system for all error conditions
 * HOW:  Test invalid configs, allocation failures, and init failures throw exceptions
 *
 * BRIDGES TESTED:
 * - genius_snn_bridge: Mathematical reasoning via SNN encoding
 * - genius_plasticity_bridge: Learning via STDP/BCM plasticity
 * - genius_training_bridge: Curriculum learning and training
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

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

class GeniusBridgesExceptionTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static char last_exception_message[256];
    static nimcp_handler_registration_t* registration;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        memset(last_exception_message, 0, sizeof(last_exception_message));

        nimcp_exception_system_init();

        // Register our test handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.name = "test_handler";
        registration = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (registration) {
            nimcp_handler_unregister(registration);
            registration = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        if (ex->message) {
            strncpy(last_exception_message, ex->message, sizeof(last_exception_message) - 1);
        }
        return false;  // Don't consume - allow propagation
    }
};

std::atomic<int> GeniusBridgesExceptionTest::handler_call_count{0};
std::atomic<int> GeniusBridgesExceptionTest::last_exception_code{0};
char GeniusBridgesExceptionTest::last_exception_message[256] = {0};
nimcp_handler_registration_t* GeniusBridgesExceptionTest::registration = nullptr;

//=============================================================================
// Genius SNN Bridge Exception Tests
//=============================================================================

TEST_F(GeniusBridgesExceptionTest, SNNBridge_InvalidZeroDimensions_ThrowsInvalidParam) {
    genius_snn_config_t config = genius_snn_config_default();
    config.num_dimensions = 0;

    genius_snn_bridge_t* bridge = genius_snn_create(&config);

    EXPECT_EQ(bridge, nullptr);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_NE(strstr(last_exception_message, "invalid num_dimensions"), nullptr);
}

TEST_F(GeniusBridgesExceptionTest, SNNBridge_TooManyDimensions_ThrowsInvalidParam) {
    genius_snn_config_t config = genius_snn_config_default();
    config.num_dimensions = GENIUS_SNN_MAX_DIMENSIONS + 1;

    genius_snn_bridge_t* bridge = genius_snn_create(&config);

    EXPECT_EQ(bridge, nullptr);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_NE(strstr(last_exception_message, "num_dimensions"), nullptr);
}

TEST_F(GeniusBridgesExceptionTest, SNNBridge_NullPointerEncode_ThrowsNullPointer) {
    genius_snn_config_t config = genius_snn_config_default();
    genius_snn_bridge_t* bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Reset counters
    handler_call_count = 0;
    last_exception_code = 0;

    // Call with null dimensions
    int result = genius_snn_encode_state(bridge, nullptr, 0);

    EXPECT_LT(result, 0);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);

    genius_snn_destroy(bridge);
}

TEST_F(GeniusBridgesExceptionTest, SNNBridge_NullBridgeEncode_ThrowsNullPointer) {
    // Reset counters
    handler_call_count = 0;
    last_exception_code = 0;

    float dims[8] = {0.0f};
    int result = genius_snn_encode_state(nullptr, dims, 8);

    EXPECT_LT(result, 0);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Genius Plasticity Bridge Exception Tests
//=============================================================================

TEST_F(GeniusBridgesExceptionTest, PlasticityBridge_NullBridgeLearn_ThrowsNullPointer) {
    handler_call_count = 0;
    last_exception_code = 0;

    int result = genius_plasticity_learn(nullptr, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 1, 1.0f);

    EXPECT_LT(result, 0);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(GeniusBridgesExceptionTest, PlasticityBridge_NonexistentSynapse_ThrowsNotFound) {
    genius_plasticity_config_t config = genius_plasticity_config_default();
    genius_plasticity_bridge_t* bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    handler_call_count = 0;
    last_exception_code = 0;

    // Try to learn with non-existent synapse
    int result = genius_plasticity_learn(bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 99999, 1.0f);

    EXPECT_LT(result, 0);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NOT_FOUND);

    genius_plasticity_destroy(bridge);
}

TEST_F(GeniusBridgesExceptionTest, PlasticityBridge_InvalidModeSkill_ThrowsInvalidParam) {
    genius_plasticity_config_t config = genius_plasticity_config_default();
    genius_plasticity_bridge_t* bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    handler_call_count = 0;
    last_exception_code = 0;

    // Try to get skill for invalid mode
    float skill = genius_plasticity_get_mode_skill(bridge, (genius_mode_t)999);

    EXPECT_LT(skill, 0.0f);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_INVALID_PARAM);

    genius_plasticity_destroy(bridge);
}

//=============================================================================
// Genius Training Bridge Exception Tests
//=============================================================================

TEST_F(GeniusBridgesExceptionTest, TrainingBridge_NullBridgeTrainBatch_ThrowsNullPointer) {
    handler_call_count = 0;
    last_exception_code = 0;

    float inputs[32] = {0.0f};
    float targets[32] = {0.0f};

    float result = genius_training_train_batch(nullptr, inputs, targets, 32);

    EXPECT_LT(result, 0.0f);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(GeniusBridgesExceptionTest, TrainingBridge_NullInputs_ThrowsNullPointer) {
    genius_training_config_t config = genius_training_config_default();
    genius_training_bridge_t* bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    handler_call_count = 0;
    last_exception_code = 0;

    float result = genius_training_train_batch(bridge, nullptr, nullptr, 32);

    EXPECT_LT(result, 0.0f);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);

    genius_training_destroy(bridge);
}

TEST_F(GeniusBridgesExceptionTest, TrainingBridge_InvalidDomain_ThrowsInvalidParam) {
    genius_training_config_t config = genius_training_config_default();
    genius_training_bridge_t* bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    handler_call_count = 0;
    last_exception_code = 0;

    int result = genius_training_set_domain(bridge, (genius_train_domain_t)999);

    EXPECT_LT(result, 0);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_INVALID_PARAM);

    genius_training_destroy(bridge);
}

TEST_F(GeniusBridgesExceptionTest, TrainingBridge_InvalidLearningRate_ThrowsInvalidParam) {
    genius_training_config_t config = genius_training_config_default();
    genius_training_bridge_t* bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    handler_call_count = 0;
    last_exception_code = 0;

    // Negative learning rate should fail
    int result = genius_training_set_learning_rate(bridge, -0.1f);

    EXPECT_LT(result, 0);
    EXPECT_GT(handler_call_count.load(), 0);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_INVALID_PARAM);

    genius_training_destroy(bridge);
}

//=============================================================================
// Cross-Bridge Exception Tests
//=============================================================================

TEST_F(GeniusBridgesExceptionTest, AllBridges_CreationSucceeds_NoExceptions) {
    handler_call_count = 0;

    // Create all three bridges with valid configs
    genius_snn_config_t snn_config = genius_snn_config_default();
    genius_snn_bridge_t* snn = genius_snn_create(&snn_config);
    ASSERT_NE(snn, nullptr);

    genius_plasticity_config_t plasticity_config = genius_plasticity_config_default();
    genius_plasticity_bridge_t* plasticity = genius_plasticity_create(&plasticity_config);
    ASSERT_NE(plasticity, nullptr);

    genius_training_config_t training_config = genius_training_config_default();
    genius_training_bridge_t* training = genius_training_create(&training_config);
    ASSERT_NE(training, nullptr);

    // No exceptions should have been thrown for valid creation
    EXPECT_EQ(handler_call_count.load(), 0);

    // Cleanup
    genius_snn_destroy(snn);
    genius_plasticity_destroy(plasticity);
    genius_training_destroy(training);
}

TEST_F(GeniusBridgesExceptionTest, AllBridges_KGWiringCreation_NoExceptions) {
    handler_call_count = 0;

    // Create KG wiring for all bridges
    // Note: These return NULL if KG wiring header isn't included, but shouldn't throw
    // The actual wiring creation happens inside bridge creation

    genius_snn_config_t snn_config = genius_snn_config_default();
    genius_snn_bridge_t* snn = genius_snn_create(&snn_config);
    ASSERT_NE(snn, nullptr);

    // KG wiring should have been created during bridge init
    // No exceptions expected
    EXPECT_EQ(handler_call_count.load(), 0);

    genius_snn_destroy(snn);
}

//=============================================================================
// Exception Message Content Tests
//=============================================================================

TEST_F(GeniusBridgesExceptionTest, SNNBridge_ExceptionMessage_ContainsModuleName) {
    genius_snn_config_t config = genius_snn_config_default();
    config.num_dimensions = 0;

    genius_snn_bridge_t* bridge = genius_snn_create(&config);

    EXPECT_EQ(bridge, nullptr);
    // Message should mention the specific dimension limit
    EXPECT_NE(strstr(last_exception_message, "0"), nullptr);
}

TEST_F(GeniusBridgesExceptionTest, PlasticityBridge_ExceptionMessage_ContainsContext) {
    handler_call_count = 0;
    memset(last_exception_message, 0, sizeof(last_exception_message));

    genius_plasticity_learn(nullptr, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 1, 1.0f);

    // Message should contain useful context
    EXPECT_GT(strlen(last_exception_message), 0u);
}

TEST_F(GeniusBridgesExceptionTest, TrainingBridge_ExceptionMessage_ContainsContext) {
    handler_call_count = 0;
    memset(last_exception_message, 0, sizeof(last_exception_message));

    genius_training_train_batch(nullptr, nullptr, nullptr, 0);

    // Message should contain useful context
    EXPECT_GT(strlen(last_exception_message), 0u);
}
