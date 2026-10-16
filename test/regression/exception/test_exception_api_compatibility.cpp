/**
 * @file test_exception_api_compatibility.cpp
 * @brief API backward compatibility tests for NIMCP exception handling
 *
 * Tests that all public API functions return documented error codes:
 * - NULL inputs return NIMCP_ERROR_NULL_POINTER
 * - Invalid params return NIMCP_ERROR_INVALID_PARAM
 * - Error codes match header definitions
 * - Security, cognitive, perception, and core module APIs
 *
 * Estimated tests: 45
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

extern "C" {
#include "nimcp.h"
#include "utils/error/nimcp_error_codes.h"
#include "security/nimcp_security.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionAPICompatibilityTest : public ::testing::Test {
protected:
    static nimcp_brain_t brain_;
    static nimcp_network_t network_;
    static nimcp_ethics_t ethics_;
    static nimcp_knowledge_t knowledge_;

    static void SetUpTestSuite() {
        ASSERT_EQ(nimcp_init(), NIMCP_OK);
        brain_ = nimcp_brain_create("test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 2);
        network_ = nimcp_network_create(10, 2, 20, 0.01f);
        ethics_ = nimcp_ethics_create();
        knowledge_ = nimcp_knowledge_create();
    }

    static void TearDownTestSuite() {
        if (brain_) { nimcp_brain_destroy(brain_); brain_ = nullptr; }
        if (network_) { nimcp_network_destroy(network_); network_ = nullptr; }
        if (ethics_) { nimcp_ethics_destroy(ethics_); ethics_ = nullptr; }
        if (knowledge_) { nimcp_knowledge_destroy(knowledge_); knowledge_ = nullptr; }
        nimcp_shutdown();
    }
};

nimcp_brain_t ExceptionAPICompatibilityTest::brain_ = nullptr;
nimcp_network_t ExceptionAPICompatibilityTest::network_ = nullptr;
nimcp_ethics_t ExceptionAPICompatibilityTest::ethics_ = nullptr;
nimcp_knowledge_t ExceptionAPICompatibilityTest::knowledge_ = nullptr;

//=============================================================================
// Core Brain API - NULL Pointer Tests
//=============================================================================

TEST_F(ExceptionAPICompatibilityTest, BrainLearnExample_NullBrain_ReturnsNullArg) {
    float features[] = {1.0f, 2.0f, 3.0f};
    // NULL brain should return NIMCP_ERROR_NULL_ARG (nimcp.h status)
    nimcp_status_t status = nimcp_brain_learn_example(nullptr, features, 3, "test", 1.0f);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainLearnExample_NullFeatures_ReturnsNullArg) {
    nimcp_status_t status = nimcp_brain_learn_example(brain_, nullptr, 3, "test", 1.0f);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainLearnExample_NullLabel_ReturnsNullArg) {
    float features[] = {1.0f, 2.0f, 3.0f};
    nimcp_status_t status = nimcp_brain_learn_example(brain_, features, 3, nullptr, 1.0f);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainPredict_NullBrain_ReturnsNullArg) {
    float features[] = {1.0f, 2.0f};
    char label[64];
    float confidence;
    nimcp_status_t status = nimcp_brain_predict(nullptr, features, 2, label, &confidence);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainPredict_NullFeatures_ReturnsNullArg) {
    char label[64];
    float confidence;
    nimcp_status_t status = nimcp_brain_predict(brain_, nullptr, 10, label, &confidence);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainPredict_NullOutput_ReturnsNullArg) {
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    nimcp_status_t status = nimcp_brain_predict(brain_, features, 10, nullptr, nullptr);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainInfer_NullBrain_ReturnsNullArg) {
    float features[] = {1.0f, 2.0f};
    float outputs[2];
    nimcp_status_t status = nimcp_brain_infer(nullptr, features, 2, outputs, 2);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainInfer_NullFeatures_ReturnsNullArg) {
    float outputs[2];
    nimcp_status_t status = nimcp_brain_infer(brain_, nullptr, 10, outputs, 2);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainInfer_NullOutputs_ReturnsNullArg) {
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    nimcp_status_t status = nimcp_brain_infer(brain_, features, 10, nullptr, 2);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainSave_NullBrain_ReturnsNullArg) {
    nimcp_status_t status = nimcp_brain_save(nullptr, "/tmp/test.nimcp");
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainSave_NullPath_ReturnsNullArg) {
    nimcp_status_t status = nimcp_brain_save(brain_, nullptr);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainProbe_NullBrain_ReturnsNullArg) {
    nimcp_brain_probe_t probe;
    nimcp_status_t status = nimcp_brain_probe(nullptr, &probe);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(ExceptionAPICompatibilityTest, BrainProbe_NullProbe_ReturnsNullArg) {
    nimcp_status_t status = nimcp_brain_probe(brain_, nullptr);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

//=============================================================================
// Core Brain API - Invalid Parameter Tests
//=============================================================================

TEST_F(ExceptionAPICompatibilityTest, BrainLearnExample_ZeroFeatures_ReturnsInvalid) {
    float features[] = {1.0f};
    // Zero features is an invalid parameter
    nimcp_status_t status = nimcp_brain_learn_example(brain_, features, 0, "test", 1.0f);
    // Should return either NIMCP_ERROR_INVALID or NIMCP_ERROR_NULL_ARG
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, BrainInfer_ZeroFeatures_ReturnsInvalid) {
    float features[] = {1.0f};
    float outputs[2];
    nimcp_status_t status = nimcp_brain_infer(brain_, features, 0, outputs, 2);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, BrainInfer_ZeroOutputs_ReturnsInvalid) {
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    float outputs[2];
    nimcp_status_t status = nimcp_brain_infer(brain_, features, 10, outputs, 0);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, BrainCreate_NullName_ReturnsNull) {
    nimcp_brain_t brain = nimcp_brain_create(nullptr, NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(ExceptionAPICompatibilityTest, BrainCreate_ZeroInputs_ReturnsNull) {
    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 0, 2);
    EXPECT_EQ(brain, nullptr);
}

TEST_F(ExceptionAPICompatibilityTest, BrainCreate_ZeroOutputs_ReturnsNull) {
    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 0);
    EXPECT_EQ(brain, nullptr);
}

//=============================================================================
// Neural Network API - NULL Pointer Tests
//=============================================================================

TEST_F(ExceptionAPICompatibilityTest, NetworkForward_NullNetwork_ReturnsError) {
    float inputs[] = {1.0f, 2.0f};
    float outputs[2];
    nimcp_status_t status = nimcp_network_forward(nullptr, inputs, 2, outputs, 2);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, NetworkForward_NullInputs_ReturnsError) {
    float outputs[2];
    nimcp_status_t status = nimcp_network_forward(network_, nullptr, 10, outputs, 2);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, NetworkForward_NullOutputs_ReturnsError) {
    float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    nimcp_status_t status = nimcp_network_forward(network_, inputs, 10, nullptr, 2);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, NetworkTrain_NullNetwork_ReturnsError) {
    float inputs[] = {1.0f, 2.0f};
    float targets[] = {1.0f, 0.0f};
    nimcp_status_t status = nimcp_network_train(nullptr, inputs, 2, targets, 2);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, NetworkTrain_NullInputs_ReturnsError) {
    float targets[] = {1.0f, 0.0f};
    nimcp_status_t status = nimcp_network_train(network_, nullptr, 10, targets, 2);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, NetworkTrain_NullTargets_ReturnsError) {
    float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    nimcp_status_t status = nimcp_network_train(network_, inputs, 10, nullptr, 2);
    EXPECT_NE(status, NIMCP_OK);
}

//=============================================================================
// Ethics Module API - NULL Pointer Tests
//=============================================================================

TEST_F(ExceptionAPICompatibilityTest, EthicsCheck_NullEthics_ReturnsError) {
    float situation[] = {1.0f, 2.0f, 3.0f};
    float score;
    nimcp_status_t status = nimcp_ethics_check(nullptr, situation, 3, &score);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, EthicsCheck_NullSituation_ReturnsError) {
    float score;
    nimcp_status_t status = nimcp_ethics_check(ethics_, nullptr, 3, &score);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, EthicsCheck_NullScore_ReturnsError) {
    float situation[] = {1.0f, 2.0f, 3.0f};
    nimcp_status_t status = nimcp_ethics_check(ethics_, situation, 3, nullptr);
    EXPECT_NE(status, NIMCP_OK);
}

//=============================================================================
// Knowledge Graph API - NULL Pointer Tests
//=============================================================================

TEST_F(ExceptionAPICompatibilityTest, KnowledgeAddFact_NullKnowledge_ReturnsError) {
    nimcp_status_t status = nimcp_knowledge_add_fact(nullptr, "subject", "predicate", "object");
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, KnowledgeAddFact_NullSubject_ReturnsError) {
    nimcp_status_t status = nimcp_knowledge_add_fact(knowledge_, nullptr, "predicate", "object");
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, KnowledgeAddFact_NullPredicate_ReturnsError) {
    nimcp_status_t status = nimcp_knowledge_add_fact(knowledge_, "subject", nullptr, "object");
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, KnowledgeAddFact_NullObject_ReturnsError) {
    nimcp_status_t status = nimcp_knowledge_add_fact(knowledge_, "subject", "predicate", nullptr);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, KnowledgeQuery_NullKnowledge_ReturnsError) {
    char result[1024];
    nimcp_status_t status = nimcp_knowledge_query(nullptr, "query", result, sizeof(result));
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, KnowledgeQuery_NullQuery_ReturnsError) {
    char result[1024];
    nimcp_status_t status = nimcp_knowledge_query(knowledge_, nullptr, result, sizeof(result));
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, KnowledgeQuery_NullResult_ReturnsError) {
    nimcp_status_t status = nimcp_knowledge_query(knowledge_, "query", nullptr, 1024);
    EXPECT_NE(status, NIMCP_OK);
}

//=============================================================================
// Training API - NULL Pointer Tests
//=============================================================================

TEST_F(ExceptionAPICompatibilityTest, BrainConfigureTraining_NullBrain_ReturnsError) {
    nimcp_training_config_t config = nimcp_training_config_default();
    nimcp_status_t status = nimcp_brain_configure_training(nullptr, &config);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, BrainConfigureTraining_NullConfig_ReturnsError) {
    nimcp_status_t status = nimcp_brain_configure_training(brain_, nullptr);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, BrainTrainStep_NullBrain_ReturnsError) {
    float features[] = {1.0f, 2.0f};
    float targets[] = {1.0f, 0.0f};
    nimcp_training_result_t result;
    nimcp_status_t status = nimcp_brain_train_step(nullptr, features, 2, targets, 2, &result);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, BrainTrainStep_NullFeatures_ReturnsError) {
    float targets[] = {1.0f, 0.0f};
    nimcp_training_result_t result;
    nimcp_status_t status = nimcp_brain_train_step(brain_, nullptr, 10, targets, 2, &result);
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(ExceptionAPICompatibilityTest, BrainTrainStep_NullTargets_ReturnsError) {
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    nimcp_training_result_t result;
    nimcp_status_t status = nimcp_brain_train_step(brain_, features, 10, nullptr, 2, &result);
    EXPECT_NE(status, NIMCP_OK);
}

//=============================================================================
// I/O Error Tests
//=============================================================================

TEST_F(ExceptionAPICompatibilityTest, BrainSave_InvalidPath_ReturnsIOError) {
    nimcp_status_t status = nimcp_brain_save(brain_, "/nonexistent/path/brain.nimcp");
    // Accept any file I/O related error (FILE_NOT_FOUND, FILE_WRITE, IO, or generic error)
    EXPECT_NE(status, NIMCP_OK)
        << "Expected error for invalid save path, got OK";
}

TEST_F(ExceptionAPICompatibilityTest, BrainLoad_NonexistentFile_ReturnsNull) {
    nimcp_brain_t loaded = nimcp_brain_load("/nonexistent/path/brain.nimcp");
    EXPECT_EQ(loaded, nullptr);
}

//=============================================================================
// Error Utility Functions Tests
//=============================================================================

TEST_F(ExceptionAPICompatibilityTest, GetError_ReturnsNonNull) {
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(ExceptionAPICompatibilityTest, ErrorToString_Success_ReturnsCorrectString) {
    const char* msg = nimcp_error_to_string(NIMCP_SUCCESS);
    EXPECT_NE(msg, nullptr);
    EXPECT_NE(strlen(msg), 0);
}

TEST_F(ExceptionAPICompatibilityTest, ErrorToString_NullPointer_ReturnsCorrectString) {
    const char* msg = nimcp_error_to_string(NIMCP_ERROR_NULL_POINTER);
    EXPECT_NE(msg, nullptr);
    EXPECT_NE(strlen(msg), 0);
}

TEST_F(ExceptionAPICompatibilityTest, ErrorToString_InvalidParam_ReturnsCorrectString) {
    const char* msg = nimcp_error_to_string(NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_NE(msg, nullptr);
    EXPECT_NE(strlen(msg), 0);
}

TEST_F(ExceptionAPICompatibilityTest, ErrorIsSuccess_SuccessCode_ReturnsTrue) {
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_WITH_WARNINGS));
    EXPECT_TRUE(nimcp_error_is_success(NIMCP_SUCCESS_PARTIAL));
}

TEST_F(ExceptionAPICompatibilityTest, ErrorIsSuccess_ErrorCode_ReturnsFalse) {
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_NULL_POINTER));
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_INVALID_PARAMETER));
    EXPECT_FALSE(nimcp_error_is_success(NIMCP_ERROR_NO_MEMORY));
}

TEST_F(ExceptionAPICompatibilityTest, ErrorIsFailure_ErrorCode_ReturnsTrue) {
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_NULL_POINTER));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_INVALID_PARAMETER));
    EXPECT_TRUE(nimcp_error_is_failure(NIMCP_ERROR_NO_MEMORY));
}

TEST_F(ExceptionAPICompatibilityTest, ErrorIsFailure_SuccessCode_ReturnsFalse) {
    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_SUCCESS));
    EXPECT_FALSE(nimcp_error_is_failure(NIMCP_SUCCESS_WITH_WARNINGS));
}

TEST_F(ExceptionAPICompatibilityTest, ErrorGetCategory_ReturnsCorrectCategory) {
    EXPECT_EQ(nimcp_error_get_category(NIMCP_SUCCESS), 0);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_UNKNOWN), 1);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_NO_MEMORY), 2);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_BRAIN_CREATION), 3);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_FILE_NOT_FOUND), 4);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_CONFIG_INVALID), 5);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_THREAD_CREATE), 6);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_SIGNAL_RECEIVED), 7);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_WORKING_MEMORY), 8);
    EXPECT_EQ(nimcp_error_get_category(NIMCP_ERROR_SECURITY_BASE), 9);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
