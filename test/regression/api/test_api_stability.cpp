/**
 * @file test_api_stability.cpp
 * @brief API stability regression tests for NIMCP
 *
 * Tests API stability to detect breaking changes:
 * - Version number format stability
 * - Function signatures unchanged
 * - Return code values unchanged
 * - Enum values unchanged
 * - Handle types remain opaque
 * - Backward compatibility of public API
 *
 * Estimated tests: 20
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <cstdio>

class APIStabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// Version Number Format Stability
//=============================================================================

TEST_F(APIStabilityTest, VersionString_HasExpectedFormat) {
    const char* version = nimcp_version();

    ASSERT_NE(version, nullptr);
    EXPECT_NE(strlen(version), 0);

    // Should be in format "X.Y.Z"
    int major, minor, patch;
    int parsed = sscanf(version, "%d.%d.%d", &major, &minor, &patch);
    EXPECT_EQ(parsed, 3);

    // Major version should be 2 (stable API)
    EXPECT_EQ(major, 2);

    // All version numbers should be non-negative
    EXPECT_GE(major, 0);
    EXPECT_GE(minor, 0);
    EXPECT_GE(patch, 0);
}

TEST_F(APIStabilityTest, VersionInt_MatchesConstants) {
    int version_int = nimcp_version_int();

    // Calculate expected version from constants
    int expected = NIMCP_VERSION_MAJOR * 10000 + NIMCP_VERSION_MINOR * 100 + NIMCP_VERSION_PATCH;

    EXPECT_EQ(version_int, expected);
}

TEST_F(APIStabilityTest, VersionConstants_HaveExpectedValues) {
    // Verify version constants exist and have reasonable values
    EXPECT_EQ(NIMCP_VERSION_MAJOR, 2);
    EXPECT_GE(NIMCP_VERSION_MINOR, 0);
    EXPECT_LE(NIMCP_VERSION_MINOR, 99);
    EXPECT_GE(NIMCP_VERSION_PATCH, 0);
    EXPECT_LE(NIMCP_VERSION_PATCH, 99);
}

TEST_F(APIStabilityTest, VersionString_MatchesConstants) {
    const char* version = nimcp_version();

    // Parse version string
    int major, minor, patch;
    sscanf(version, "%d.%d.%d", &major, &minor, &patch);

    // Should match constants
    EXPECT_EQ(major, NIMCP_VERSION_MAJOR);
    EXPECT_EQ(minor, NIMCP_VERSION_MINOR);
    EXPECT_EQ(patch, NIMCP_VERSION_PATCH);
}

//=============================================================================
// Function Signatures Unchanged
//=============================================================================

TEST_F(APIStabilityTest, BrainCreate_SignatureStable) {
    // Verify nimcp_brain_create accepts expected parameters
    nimcp_brain_t brain = nimcp_brain_create(
        "test",                          // const char* name
        NIMCP_BRAIN_SMALL,               // nimcp_brain_size_t size
        NIMCP_TASK_CLASSIFICATION,       // nimcp_brain_task_t task
        10,                              // uint32_t num_inputs
        2                                // uint32_t num_outputs
    );

    EXPECT_NE(brain, nullptr);
    nimcp_brain_destroy(brain);
}

TEST_F(APIStabilityTest, BrainLearn_SignatureStable) {
    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // Verify nimcp_brain_learn_example signature
    nimcp_status_t status = nimcp_brain_learn_example(
        brain,           // nimcp_brain_t brain
        features,        // const float* features
        5,               // uint32_t num_features
        "label",         // const char* label
        1.0f             // float confidence
    );

    EXPECT_EQ(status, NIMCP_SUCCESS);
    nimcp_brain_destroy(brain);
}

TEST_F(APIStabilityTest, BrainPredict_SignatureStable) {
    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    char label[64];
    float confidence;

    // Verify nimcp_brain_predict signature
    nimcp_status_t status = nimcp_brain_predict(
        brain,           // nimcp_brain_t brain
        features,        // const float* features
        5,               // uint32_t num_features
        label,           // char* out_label
        &confidence      // float* out_confidence
    );

    // May succeed or fail, but signature should compile
    (void)status;

    nimcp_brain_destroy(brain);
}

TEST_F(APIStabilityTest, BrainInfer_SignatureStable) {
    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, NIMCP_TASK_REGRESSION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float outputs[2];

    // Verify nimcp_brain_infer signature
    nimcp_status_t status = nimcp_brain_infer(
        brain,           // nimcp_brain_t brain
        features,        // const float* features
        5,               // uint32_t num_features
        outputs,         // float* outputs
        2                // uint32_t num_outputs
    );

    (void)status;
    nimcp_brain_destroy(brain);
}

TEST_F(APIStabilityTest, BrainSaveLoad_SignatureStable) {
    const char* path = "/tmp/api_stability_test.nimcp";

    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // Verify nimcp_brain_save signature
    nimcp_status_t save_status = nimcp_brain_save(
        brain,           // nimcp_brain_t brain
        path             // const char* filepath
    );

    if (save_status == NIMCP_SUCCESS) {
        nimcp_brain_destroy(brain);

        // Verify nimcp_brain_load signature
        nimcp_brain_t loaded = nimcp_brain_load(
            path         // const char* filepath
        );

        if (loaded) {
            nimcp_brain_destroy(loaded);
        }

        unlink(path);
    } else {
        nimcp_brain_destroy(brain);
    }
}

//=============================================================================
// Return Code Values Unchanged
//=============================================================================

TEST_F(APIStabilityTest, StatusCodes_HaveExpectedValues) {
    // Verify enum values haven't changed
    EXPECT_EQ(NIMCP_SUCCESS, 0);
    EXPECT_EQ(NIMCP_ERROR, 1000);
    EXPECT_EQ(NIMCP_ERROR_NULL_ARG, 1003);
    EXPECT_EQ(NIMCP_ERROR_INVALID, 1004);
    EXPECT_EQ(NIMCP_ERROR_MEMORY, 2000);
    EXPECT_EQ(NIMCP_ERROR_IO, 4000);
}

TEST_F(APIStabilityTest, StatusCodes_ReturnedCorrectly) {
    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // Test NULL pointer returns NIMCP_ERROR_NULL_ARG
    nimcp_status_t status = nimcp_brain_learn_example(brain, nullptr, 5, "test", 1.0f);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    status = nimcp_brain_predict(brain, nullptr, 5, nullptr, nullptr);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    // Test invalid path returns NIMCP_ERROR_IO
    status = nimcp_brain_save(brain, "/invalid/path/brain.nimcp");
    EXPECT_EQ(status, NIMCP_ERROR_IO);

    nimcp_brain_destroy(brain);
}

TEST_F(APIStabilityTest, StatusCodes_NetworkAPI) {
    nimcp_network_t network = nimcp_network_create(5, 2, 20, 0.01f);
    ASSERT_NE(network, nullptr);

    // Test NULL pointer returns error
    nimcp_status_t status = nimcp_network_forward(network, nullptr, 5, nullptr, 2);
    EXPECT_NE(status, NIMCP_SUCCESS);

    nimcp_network_destroy(network);
}

//=============================================================================
// Enum Values Unchanged
//=============================================================================

TEST_F(APIStabilityTest, BrainSize_EnumValuesStable) {
    // Verify brain size enum values
    EXPECT_EQ(NIMCP_BRAIN_TINY, 0);
    EXPECT_EQ(NIMCP_BRAIN_SMALL, 1);
    EXPECT_EQ(NIMCP_BRAIN_MEDIUM, 2);
    EXPECT_EQ(NIMCP_BRAIN_LARGE, 3);
}

TEST_F(APIStabilityTest, BrainTask_EnumValuesStable) {
    // Verify brain task enum values
    EXPECT_EQ(NIMCP_TASK_CLASSIFICATION, 0);
    EXPECT_EQ(NIMCP_TASK_REGRESSION, 1);
    EXPECT_EQ(NIMCP_TASK_PATTERN_MATCHING, 2);
    EXPECT_EQ(NIMCP_TASK_SEQUENCE, 3);
    EXPECT_EQ(NIMCP_TASK_ASSOCIATION, 4);
}

TEST_F(APIStabilityTest, CognitiveModule_EnumValuesStable) {
    // Verify cognitive module enum values
    EXPECT_EQ(NIMCP_MODULE_NONE, 0);
    EXPECT_EQ(NIMCP_MODULE_PERCEPTION, 1);
    EXPECT_EQ(NIMCP_MODULE_WORKING_MEMORY, 2);
    EXPECT_EQ(NIMCP_MODULE_EXECUTIVE, 3);
    EXPECT_EQ(NIMCP_MODULE_THEORY_OF_MIND, 4);
    EXPECT_EQ(NIMCP_MODULE_ETHICS, 5);
    EXPECT_EQ(NIMCP_MODULE_ATTENTION, 6);
    EXPECT_EQ(NIMCP_MODULE_EMOTION, 7);

    // Verify custom start value
    EXPECT_EQ(NIMCP_MODULE_CUSTOM_START, 100);
}

TEST_F(APIStabilityTest, EnumValues_CreateBrainWithAllSizes) {
    // Verify all brain sizes can be used
    nimcp_brain_t tiny = nimcp_brain_create("tiny", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_brain_t small = nimcp_brain_create("small", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_brain_t medium = nimcp_brain_create("medium", NIMCP_BRAIN_MEDIUM, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_brain_t large = nimcp_brain_create("large", NIMCP_BRAIN_LARGE, NIMCP_TASK_CLASSIFICATION, 5, 2);

    EXPECT_NE(tiny, nullptr);
    EXPECT_NE(small, nullptr);
    EXPECT_NE(medium, nullptr);
    EXPECT_NE(large, nullptr);

    nimcp_brain_destroy(large);
    nimcp_brain_destroy(medium);
    nimcp_brain_destroy(small);
    nimcp_brain_destroy(tiny);
}

TEST_F(APIStabilityTest, EnumValues_CreateBrainWithAllTasks) {
    // Verify all brain tasks can be used
    std::vector<nimcp_brain_t> brains;

    brains.push_back(nimcp_brain_create("class", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2));
    brains.push_back(nimcp_brain_create("regr", NIMCP_BRAIN_SMALL, NIMCP_TASK_REGRESSION, 5, 1));
    brains.push_back(nimcp_brain_create("pattern", NIMCP_BRAIN_SMALL, NIMCP_TASK_PATTERN_MATCHING, 16, 2));
    brains.push_back(nimcp_brain_create("seq", NIMCP_BRAIN_SMALL, NIMCP_TASK_SEQUENCE, 8, 4));
    brains.push_back(nimcp_brain_create("assoc", NIMCP_BRAIN_SMALL, NIMCP_TASK_ASSOCIATION, 10, 5));

    for (auto brain : brains) {
        EXPECT_NE(brain, nullptr);
        nimcp_brain_destroy(brain);
    }
}

//=============================================================================
// Handle Types Remain Opaque
//=============================================================================

TEST_F(APIStabilityTest, Handles_AreOpaquePointers) {
    // Handles should be pointer types (can assign nullptr)
    nimcp_brain_t brain = nullptr;
    nimcp_network_t network = nullptr;
    nimcp_ethics_t ethics = nullptr;
    nimcp_knowledge_t knowledge = nullptr;
    nimcp_brain_snapshot_t snapshot = nullptr;

    // Should compile and be nullptr
    EXPECT_EQ(brain, nullptr);
    EXPECT_EQ(network, nullptr);
    EXPECT_EQ(ethics, nullptr);
    EXPECT_EQ(knowledge, nullptr);
    EXPECT_EQ(snapshot, nullptr);
}

TEST_F(APIStabilityTest, Handles_CanBeCompared) {
    nimcp_brain_t brain1 = nimcp_brain_create("test1", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_brain_t brain2 = nimcp_brain_create("test2", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);

    // Should be able to compare handles
    EXPECT_NE(brain1, brain2);

    nimcp_brain_destroy(brain2);
    nimcp_brain_destroy(brain1);
}

TEST_F(APIStabilityTest, Handles_CanBeStoredInContainers) {
    // Verify handles can be stored in standard containers
    std::vector<nimcp_brain_t> brains;

    for (int i = 0; i < 5; i++) {
        nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 5, 2);
        if (brain) {
            brains.push_back(brain);
        }
    }

    EXPECT_GT(brains.size(), 0);

    for (auto brain : brains) {
        nimcp_brain_destroy(brain);
    }
}

//=============================================================================
// Backward Compatibility
//=============================================================================

TEST_F(APIStabilityTest, BackwardCompat_BasicAPIWorks) {
    // Test basic API that should never break
    nimcp_brain_t brain = nimcp_brain_create("compat", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);

    char label[64];
    float confidence;
    nimcp_brain_predict(brain, features, 5, label, &confidence);

    nimcp_brain_destroy(brain);
}

TEST_F(APIStabilityTest, BackwardCompat_AllModuleTypesWork) {
    // All module types from original API should work
    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    nimcp_network_t network = nimcp_network_create(5, 2, 20, 0.01f);
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();

    EXPECT_NE(brain, nullptr);
    EXPECT_NE(network, nullptr);
    EXPECT_NE(ethics, nullptr);
    EXPECT_NE(knowledge, nullptr);

    nimcp_knowledge_destroy(knowledge);
    nimcp_ethics_destroy(ethics);
    nimcp_network_destroy(network);
    nimcp_brain_destroy(brain);
}

TEST_F(APIStabilityTest, BackwardCompat_ErrorHandling) {
    // Error handling should be consistent
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);

    // Creating brain with NULL name should fail
    nimcp_brain_t brain = nimcp_brain_create(nullptr, NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    EXPECT_EQ(brain, nullptr);

    // Error message should be set
    error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
    EXPECT_GT(strlen(error), 0);
}

TEST_F(APIStabilityTest, BackwardCompat_InitShutdownSequence) {
    // Can call init/shutdown multiple times
    nimcp_shutdown();
    ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);

    // Multiple init calls should be safe
    EXPECT_EQ(nimcp_init(), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_init(), NIMCP_SUCCESS);

    // Multiple shutdown calls should be safe
    nimcp_shutdown();
    nimcp_shutdown();

    // Re-initialize for other tests
    ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
