/**
 * @file test_batch_api.cpp
 * @brief Unit tests for batch API (v2.7.0 Enhancement 2)
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>

extern "C" {
#include "include/nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
}

class BatchAPITest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_init();

        // Create a small brain for testing
        brain = nimcp_brain_create("test_brain", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            nimcp_brain_destroy(brain);
        }
        nimcp_shutdown();
    }
};

//=============================================================================
// Batch Prediction Tests
//=============================================================================

TEST_F(BatchAPITest, BatchPredictNullBrain) {
    float features[] = {1.0f, 2.0f, 3.0f};
    const float* feature_ptrs[] = {features};
    char* labels[] = {(char*)"label1"};
    float confidences[1];

    nimcp_status_t status = nimcp_brain_predict_batch(
        nullptr, feature_ptrs, 3, 1, labels, confidences
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(BatchAPITest, BatchPredictNullFeatures) {
    char* labels[] = {(char*)"label1"};
    float confidences[1];

    nimcp_status_t status = nimcp_brain_predict_batch(
        brain, nullptr, 3, 1, labels, confidences
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(BatchAPITest, BatchPredictZeroBatchSize) {
    float features[] = {1.0f, 2.0f, 3.0f};
    const float* feature_ptrs[] = {features};
    char* labels[] = {(char*)"label1"};
    float confidences[1];

    nimcp_status_t status = nimcp_brain_predict_batch(
        brain, feature_ptrs, 3, 0, labels, confidences
    );

    EXPECT_EQ(status, NIMCP_ERROR_INVALID_ARG);
}

TEST_F(BatchAPITest, BatchPredictSingleSample) {
    // Single sample batch
    float features[] = {0.5f, 0.5f, 0.5f};
    const float* feature_ptrs[] = {features};

    char label_buffer[64];
    char* labels[] = {label_buffer};
    float confidences[1];

    nimcp_status_t status = nimcp_brain_predict_batch(
        brain, feature_ptrs, 3, 1, labels, confidences
    );

    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GE(confidences[0], 0.0f);
    EXPECT_LE(confidences[0], 1.0f);
    EXPECT_GT(strlen(labels[0]), 0);
}

TEST_F(BatchAPITest, BatchPredictMultipleSamples) {
    // 3 samples
    float features1[] = {0.1f, 0.2f, 0.3f};
    float features2[] = {0.4f, 0.5f, 0.6f};
    float features3[] = {0.7f, 0.8f, 0.9f};

    const float* feature_ptrs[] = {features1, features2, features3};

    char label_buffer1[64];
    char label_buffer2[64];
    char label_buffer3[64];
    char* labels[] = {label_buffer1, label_buffer2, label_buffer3};
    float confidences[3];

    nimcp_status_t status = nimcp_brain_predict_batch(
        brain, feature_ptrs, 3, 3, labels, confidences
    );

    EXPECT_EQ(status, NIMCP_OK);

    // Check all outputs are valid
    for (int i = 0; i < 3; i++) {
        EXPECT_GE(confidences[i], 0.0f);
        EXPECT_LE(confidences[i], 1.0f);
        EXPECT_GT(strlen(labels[i]), 0);
    }
}

TEST_F(BatchAPITest, BatchPredictNullFeatureInBatch) {
    float features1[] = {0.1f, 0.2f, 0.3f};
    const float* feature_ptrs[] = {features1, nullptr};  // Second is NULL

    char label_buffer1[64];
    char label_buffer2[64];
    char* labels[] = {label_buffer1, label_buffer2};
    float confidences[2];

    nimcp_status_t status = nimcp_brain_predict_batch(
        brain, feature_ptrs, 3, 2, labels, confidences
    );

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

//=============================================================================
// Batch Learning Tests
//=============================================================================

TEST_F(BatchAPITest, BatchLearnNullBrain) {
    float features[] = {1.0f, 2.0f, 3.0f};
    const float* feature_ptrs[] = {features};
    const char* labels[] = {"good"};

    float loss = nimcp_brain_learn_batch(
        nullptr, feature_ptrs, 3, 1, labels, nullptr
    );

    EXPECT_LT(loss, 0.0f);  // Should return error (-1)
}

TEST_F(BatchAPITest, BatchLearnNullFeatures) {
    const char* labels[] = {"good"};

    float loss = nimcp_brain_learn_batch(
        brain, nullptr, 3, 1, labels, nullptr
    );

    EXPECT_LT(loss, 0.0f);
}

TEST_F(BatchAPITest, BatchLearnZeroBatchSize) {
    float features[] = {1.0f, 2.0f, 3.0f};
    const float* feature_ptrs[] = {features};
    const char* labels[] = {"good"};

    float loss = nimcp_brain_learn_batch(
        brain, feature_ptrs, 3, 0, labels, nullptr
    );

    EXPECT_LT(loss, 0.0f);
}

TEST_F(BatchAPITest, BatchLearnSingleSample) {
    float features[] = {0.5f, 0.5f, 0.5f};
    const float* feature_ptrs[] = {features};
    const char* labels[] = {"positive"};

    float loss = nimcp_brain_learn_batch(
        brain, feature_ptrs, 3, 1, labels, nullptr
    );

    EXPECT_GE(loss, 0.0f);  // Should return valid loss
}

TEST_F(BatchAPITest, BatchLearnMultipleSamples) {
    // 4 training samples
    float features1[] = {0.1f, 0.2f, 0.3f};
    float features2[] = {0.4f, 0.5f, 0.6f};
    float features3[] = {0.7f, 0.8f, 0.9f};
    float features4[] = {0.2f, 0.3f, 0.4f};

    const float* feature_ptrs[] = {features1, features2, features3, features4};
    const char* labels[] = {"class_a", "class_b", "class_a", "class_b"};

    float loss = nimcp_brain_learn_batch(
        brain, feature_ptrs, 3, 4, labels, nullptr
    );

    EXPECT_GE(loss, 0.0f);
}

TEST_F(BatchAPITest, BatchLearnWithConfidences) {
    float features1[] = {0.1f, 0.2f, 0.3f};
    float features2[] = {0.4f, 0.5f, 0.6f};

    const float* feature_ptrs[] = {features1, features2};
    const char* labels[] = {"certain", "uncertain"};
    float confidences[] = {1.0f, 0.5f};  // Different confidence levels

    float loss = nimcp_brain_learn_batch(
        brain, feature_ptrs, 3, 2, labels, confidences
    );

    EXPECT_GE(loss, 0.0f);
}

TEST_F(BatchAPITest, BatchLearnWithNullConfidences) {
    float features[] = {0.5f, 0.5f, 0.5f};
    const float* feature_ptrs[] = {features};
    const char* labels[] = {"test"};

    // Should default to confidence 1.0
    float loss = nimcp_brain_learn_batch(
        brain, feature_ptrs, 3, 1, labels, nullptr
    );

    EXPECT_GE(loss, 0.0f);
}

//=============================================================================
// Batch Performance Tests
//=============================================================================

TEST_F(BatchAPITest, LargeBatchProcessing) {
    const int batch_size = 100;
    std::vector<float> features_storage(batch_size * 3);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<std::vector<char>> label_buffers(batch_size, std::vector<char>(64));
    std::vector<char*> labels(batch_size);
    std::vector<float> confidences(batch_size);

    // Prepare batch data
    for (int i = 0; i < batch_size; i++) {
        features_storage[i * 3 + 0] = (float)i / batch_size;
        features_storage[i * 3 + 1] = (float)i / batch_size;
        features_storage[i * 3 + 2] = (float)i / batch_size;
        feature_ptrs[i] = &features_storage[i * 3];
        labels[i] = label_buffers[i].data();
    }

    nimcp_status_t status = nimcp_brain_predict_batch(
        brain, feature_ptrs.data(), 3, batch_size,
        labels.data(), confidences.data()
    );

    EXPECT_EQ(status, NIMCP_OK);

    // Verify all predictions completed
    for (int i = 0; i < batch_size; i++) {
        EXPECT_GE(confidences[i], 0.0f);
        EXPECT_LE(confidences[i], 1.0f);
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(BatchAPITest, BatchPredictEmptyFeatures) {
    float dummy_feature = 0.0f;  // Need at least one element for array
    const float* feature_ptrs[] = {&dummy_feature};
    char label_buffer[64];
    char* labels[] = {label_buffer};
    float confidences[1];

    // 0 features per sample
    nimcp_status_t status = nimcp_brain_predict_batch(
        brain, feature_ptrs, 0, 1, labels, confidences
    );

    // Should fail or handle gracefully
    EXPECT_NE(status, NIMCP_OK);
}

TEST_F(BatchAPITest, ConsistencyWithSinglePredict) {
    // Batch of 1 should match single prediction
    float features[] = {0.5f, 0.5f, 0.5f};

    // Single prediction
    char single_label[64];
    float single_confidence;
    nimcp_status_t status1 = nimcp_brain_predict(
        brain, features, 3, single_label, &single_confidence
    );

    // Batch prediction of 1
    const float* feature_ptrs[] = {features};
    char batch_label[64];
    char* batch_labels[] = {batch_label};
    float batch_confidences[1];

    nimcp_status_t status2 = nimcp_brain_predict_batch(
        brain, feature_ptrs, 3, 1, batch_labels, batch_confidences
    );

    EXPECT_EQ(status1, NIMCP_OK);
    EXPECT_EQ(status2, NIMCP_OK);

    // Results should be similar (allowing for minor numerical differences)
    EXPECT_STREQ(single_label, batch_labels[0]);
    EXPECT_NEAR(single_confidence, batch_confidences[0], 0.01f);
}
