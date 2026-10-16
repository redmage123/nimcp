/**
 * @file test_genius_bridges_exception_regression.cpp
 * @brief Regression tests for exception handling in Mathematical Genius bridges
 * @date 2026-01-24
 *
 * WHAT: Verify exception handling behavior remains consistent across versions
 * WHY:  Prevent regressions in error reporting, codes, and messages
 * HOW:  Check exact error codes, message patterns, and handler behavior
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

class GeniusBridgesExceptionRegressionTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static int last_exception_code;
    static char last_exception_message[256];
    static char last_exception_function[128];
    static char last_exception_file[256];
    static nimcp_handler_registration_t* registration;

    void SetUp() override {
        exception_count = 0;
        last_exception_code = 0;
        memset(last_exception_message, 0, sizeof(last_exception_message));
        memset(last_exception_function, 0, sizeof(last_exception_function));
        memset(last_exception_file, 0, sizeof(last_exception_file));

        nimcp_exception_system_init();

        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.handler = capture_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.name = "regression_capture_handler";
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

    static bool capture_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_count++;
        last_exception_code = ex->code;
        if (ex->message) {
            strncpy(last_exception_message, ex->message, sizeof(last_exception_message) - 1);
        }
        if (ex->function) {
            strncpy(last_exception_function, ex->function, sizeof(last_exception_function) - 1);
        }
        if (ex->file) {
            strncpy(last_exception_file, ex->file, sizeof(last_exception_file) - 1);
        }
        return false;
    }
};

std::atomic<int> GeniusBridgesExceptionRegressionTest::exception_count{0};
int GeniusBridgesExceptionRegressionTest::last_exception_code{0};
char GeniusBridgesExceptionRegressionTest::last_exception_message[256] = {0};
char GeniusBridgesExceptionRegressionTest::last_exception_function[128] = {0};
char GeniusBridgesExceptionRegressionTest::last_exception_file[256] = {0};
nimcp_handler_registration_t* GeniusBridgesExceptionRegressionTest::registration = nullptr;

//=============================================================================
// Error Code Regression Tests - Verify exact error codes are stable
//=============================================================================

TEST_F(GeniusBridgesExceptionRegressionTest, SNNBridge_InvalidDimensions_ErrorCode1002) {
    genius_snn_config_t config = genius_snn_config_default();
    config.num_dimensions = 0;

    genius_snn_bridge_t* bridge = genius_snn_create(&config);
    EXPECT_EQ(bridge, nullptr);

    // REGRESSION: Error code must be exactly NIMCP_ERROR_INVALID_PARAM (1002)
    EXPECT_EQ(last_exception_code, 1002);
}

TEST_F(GeniusBridgesExceptionRegressionTest, SNNBridge_NullBridge_ErrorCode1003) {
    genius_snn_encode_state(nullptr, nullptr, 0);

    // REGRESSION: Error code must be exactly NIMCP_ERROR_NULL_POINTER (1003)
    EXPECT_EQ(last_exception_code, 1003);
}

TEST_F(GeniusBridgesExceptionRegressionTest, PlasticityBridge_NullBridge_ErrorCode1003) {
    genius_plasticity_learn(nullptr, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 1, 1.0f);

    // REGRESSION: Error code must be exactly NIMCP_ERROR_NULL_POINTER (1003)
    EXPECT_EQ(last_exception_code, 1003);
}

TEST_F(GeniusBridgesExceptionRegressionTest, PlasticityBridge_SynapseNotFound_ErrorCode1009) {
    genius_plasticity_config_t config = genius_plasticity_config_default();
    genius_plasticity_bridge_t* bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_learn(bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 99999, 1.0f);

    // REGRESSION: Error code must be exactly NIMCP_ERROR_NOT_FOUND (1009)
    EXPECT_EQ(last_exception_code, 1009);

    genius_plasticity_destroy(bridge);
}

TEST_F(GeniusBridgesExceptionRegressionTest, TrainingBridge_NullBridge_ErrorCode1003) {
    genius_training_train_batch(nullptr, nullptr, nullptr, 0);

    // REGRESSION: Error code must be exactly NIMCP_ERROR_NULL_POINTER (1003)
    EXPECT_EQ(last_exception_code, 1003);
}

TEST_F(GeniusBridgesExceptionRegressionTest, TrainingBridge_InvalidDomain_ErrorCode1002) {
    genius_training_config_t config = genius_training_config_default();
    genius_training_bridge_t* bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_set_domain(bridge, (genius_train_domain_t)999);

    // REGRESSION: Error code must be exactly NIMCP_ERROR_INVALID_PARAM (1002)
    EXPECT_EQ(last_exception_code, 1002);

    genius_training_destroy(bridge);
}

//=============================================================================
// Function Name Regression Tests - Verify function context is captured
//=============================================================================

TEST_F(GeniusBridgesExceptionRegressionTest, SNNBridge_Create_FunctionNameCaptured) {
    genius_snn_config_t config = genius_snn_config_default();
    config.num_dimensions = 0;

    genius_snn_create(&config);

    // REGRESSION: Function name must be captured
    EXPECT_NE(strstr(last_exception_function, "genius_snn_create"), nullptr);
}

TEST_F(GeniusBridgesExceptionRegressionTest, SNNBridge_Encode_FunctionNameCaptured) {
    genius_snn_encode_state(nullptr, nullptr, 0);

    // REGRESSION: Function name must be captured
    EXPECT_NE(strstr(last_exception_function, "genius_snn_encode_state"), nullptr);
}

TEST_F(GeniusBridgesExceptionRegressionTest, PlasticityBridge_Learn_FunctionNameCaptured) {
    genius_plasticity_learn(nullptr, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 1, 1.0f);

    // REGRESSION: Function name must be captured
    EXPECT_NE(strstr(last_exception_function, "genius_plasticity_learn"), nullptr);
}

TEST_F(GeniusBridgesExceptionRegressionTest, TrainingBridge_TrainBatch_FunctionNameCaptured) {
    genius_training_train_batch(nullptr, nullptr, nullptr, 0);

    // REGRESSION: Function name must be captured
    EXPECT_NE(strstr(last_exception_function, "genius_training_train_batch"), nullptr);
}

//=============================================================================
// Message Content Regression Tests - Verify messages contain key information
//=============================================================================

TEST_F(GeniusBridgesExceptionRegressionTest, SNNBridge_InvalidDimensions_MessageContainsDimensionInfo) {
    genius_snn_config_t config = genius_snn_config_default();
    config.num_dimensions = 0;

    genius_snn_create(&config);

    // REGRESSION: Message must contain "num_dimensions"
    EXPECT_NE(strstr(last_exception_message, "num_dimensions"), nullptr);
}

TEST_F(GeniusBridgesExceptionRegressionTest, SNNBridge_NullBridge_MessageContainsNULL) {
    genius_snn_encode_state(nullptr, nullptr, 0);

    // REGRESSION: Message must mention NULL
    EXPECT_NE(strstr(last_exception_message, "NULL"), nullptr);
}

TEST_F(GeniusBridgesExceptionRegressionTest, PlasticityBridge_SynapseNotFound_MessageContainsSynapseId) {
    genius_plasticity_config_t config = genius_plasticity_config_default();
    genius_plasticity_bridge_t* bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_learn(bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 99999, 1.0f);

    // REGRESSION: Message must contain the synapse ID
    EXPECT_NE(strstr(last_exception_message, "99999"), nullptr);

    genius_plasticity_destroy(bridge);
}

TEST_F(GeniusBridgesExceptionRegressionTest, TrainingBridge_InvalidLearningRate_MessageContainsRate) {
    genius_training_config_t config = genius_training_config_default();
    genius_training_bridge_t* bridge = genius_training_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_training_set_learning_rate(bridge, -0.1f);

    // REGRESSION: Message must mention learning rate or the value
    EXPECT_NE(strstr(last_exception_message, "learning rate"), nullptr);

    genius_training_destroy(bridge);
}

//=============================================================================
// File Path Regression Tests - Verify source file is captured
//=============================================================================

TEST_F(GeniusBridgesExceptionRegressionTest, SNNBridge_Exception_FilePathCaptured) {
    genius_snn_encode_state(nullptr, nullptr, 0);

    // REGRESSION: File path must be captured and contain expected module
    EXPECT_NE(strstr(last_exception_file, "genius_snn_bridge"), nullptr);
}

TEST_F(GeniusBridgesExceptionRegressionTest, PlasticityBridge_Exception_FilePathCaptured) {
    genius_plasticity_learn(nullptr, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 1, 1.0f);

    // REGRESSION: File path must be captured and contain expected module
    EXPECT_NE(strstr(last_exception_file, "genius_plasticity_bridge"), nullptr);
}

TEST_F(GeniusBridgesExceptionRegressionTest, TrainingBridge_Exception_FilePathCaptured) {
    genius_training_train_batch(nullptr, nullptr, nullptr, 0);

    // REGRESSION: File path must be captured and contain expected module
    EXPECT_NE(strstr(last_exception_file, "genius_training_bridge"), nullptr);
}

//=============================================================================
// Return Value Regression Tests - Verify return values on error
//=============================================================================

TEST_F(GeniusBridgesExceptionRegressionTest, SNNBridge_Create_ReturnsNullOnError) {
    genius_snn_config_t config = genius_snn_config_default();
    config.num_dimensions = 0;

    genius_snn_bridge_t* result = genius_snn_create(&config);

    // REGRESSION: Must return NULL on error
    EXPECT_EQ(result, nullptr);
}

TEST_F(GeniusBridgesExceptionRegressionTest, SNNBridge_Encode_ReturnsNegativeOnError) {
    int result = genius_snn_encode_state(nullptr, nullptr, 0);

    // REGRESSION: Must return negative on error
    EXPECT_LT(result, 0);
}

TEST_F(GeniusBridgesExceptionRegressionTest, PlasticityBridge_Learn_ReturnsNegativeOnError) {
    int result = genius_plasticity_learn(nullptr, GENIUS_LEARN_PROOF_SUCCESS, 0.5f, 1, 1.0f);

    // REGRESSION: Must return negative on error
    EXPECT_LT(result, 0);
}

TEST_F(GeniusBridgesExceptionRegressionTest, TrainingBridge_TrainBatch_ReturnsNegativeOnError) {
    float result = genius_training_train_batch(nullptr, nullptr, nullptr, 0);

    // REGRESSION: Must return negative on error
    EXPECT_LT(result, 0.0f);
}

TEST_F(GeniusBridgesExceptionRegressionTest, PlasticityBridge_GetModeSkill_ReturnsNegativeOnError) {
    genius_plasticity_config_t config = genius_plasticity_config_default();
    genius_plasticity_bridge_t* bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    float result = genius_plasticity_get_mode_skill(bridge, (genius_mode_t)999);

    // REGRESSION: Must return negative on error
    EXPECT_LT(result, 0.0f);

    genius_plasticity_destroy(bridge);
}

//=============================================================================
// Exception Count Regression Tests - Verify single exception per error
//=============================================================================

TEST_F(GeniusBridgesExceptionRegressionTest, SingleError_SingleException_NoDuplicates) {
    exception_count = 0;

    genius_snn_encode_state(nullptr, nullptr, 0);

    // REGRESSION: One error should produce exactly one exception
    EXPECT_EQ(exception_count.load(), 1);
}

TEST_F(GeniusBridgesExceptionRegressionTest, ValidOperation_NoException_CountZero) {
    exception_count = 0;

    genius_snn_config_t config = genius_snn_config_default();
    genius_snn_bridge_t* bridge = genius_snn_create(&config);
    ASSERT_NE(bridge, nullptr);

    float dims[8] = {0.5f};
    genius_snn_encode_state(bridge, dims, 8);

    // REGRESSION: Valid operation should produce no exceptions
    EXPECT_EQ(exception_count.load(), 0);

    genius_snn_destroy(bridge);
}
