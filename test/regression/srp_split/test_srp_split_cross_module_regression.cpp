/**
 * @file test_srp_split_cross_module_regression.cpp
 * @brief Cross-module regression tests for SRP-split modules
 *
 * Verifies that the #include-based SRP splits don't break interactions
 * between modules. Tests that functions in one module's part files
 * correctly interact with functions in another module's part files.
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <cmath>

class SRPSplitCrossModuleTest : public ::testing::Test {
protected:
    nimcp_brain_t brain_ = nullptr;
    nimcp_ethics_t ethics_ = nullptr;
    nimcp_knowledge_t knowledge_ = nullptr;

    void SetUp() override {
        nimcp_status_t status = nimcp_init();
        ASSERT_EQ(status, NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (brain_) nimcp_brain_destroy(brain_);
        if (ethics_) nimcp_ethics_destroy(ethics_);
        if (knowledge_) nimcp_knowledge_destroy(knowledge_);
        nimcp_shutdown();
    }
};

//=============================================================================
// Brain + Ethics interaction
//=============================================================================

TEST_F(SRPSplitCrossModuleTest, BrainAndEthicsCoexist) {
    brain_ = nimcp_brain_create(
        "cross_brain", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain_, nullptr);

    ethics_ = nimcp_ethics_create();
    ASSERT_NE(ethics_, nullptr);

    float features[10] = {0.5f, 0.3f, 0.7f, 0.2f, 0.8f,
                          0.1f, 0.9f, 0.4f, 0.6f, 0.5f};

    nimcp_status_t status = nimcp_brain_learn_example(
        brain_, features, 10, "class_a", 0.9f
    );
    EXPECT_EQ(status, NIMCP_SUCCESS);

    float score = -1.0f;
    status = nimcp_ethics_check(ethics_, features, 10, &score);
    EXPECT_EQ(status, NIMCP_SUCCESS);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);

    uint32_t neuron_count = nimcp_brain_get_neuron_count(brain_);
    EXPECT_GT(neuron_count, 0u);
}

//=============================================================================
// Brain + Knowledge interaction
//=============================================================================

TEST_F(SRPSplitCrossModuleTest, BrainAndKnowledgeCoexist) {
    brain_ = nimcp_brain_create(
        "cross_brain_k", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain_, nullptr);

    knowledge_ = nimcp_knowledge_create();
    ASSERT_NE(knowledge_, nullptr);

    float features[10] = {1.0f};
    nimcp_status_t status = nimcp_brain_learn_example(
        brain_, features, 10, "class_b", 0.8f
    );
    EXPECT_EQ(status, NIMCP_SUCCESS);

    char label[NIMCP_MAX_LABEL_SIZE] = {0};
    float confidence = 0.0f;
    status = nimcp_brain_predict(brain_, features, 10, label, &confidence);
    EXPECT_EQ(status, NIMCP_SUCCESS);
}

//=============================================================================
// All three modules simultaneously
//=============================================================================

TEST_F(SRPSplitCrossModuleTest, AllModulesCoexist) {
    brain_ = nimcp_brain_create(
        "cross_all", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ethics_ = nimcp_ethics_create();
    knowledge_ = nimcp_knowledge_create();

    ASSERT_NE(brain_, nullptr);
    ASSERT_NE(ethics_, nullptr);
    ASSERT_NE(knowledge_, nullptr);

    float features[10] = {0.5f};

    nimcp_brain_learn_example(brain_, features, 10, "class_a", 0.9f);
    uint32_t count = nimcp_brain_get_neuron_count(brain_);
    EXPECT_GT(count, 0u);

    float score = -1.0f;
    nimcp_ethics_check(ethics_, features, 10, &score);
    EXPECT_GE(score, 0.0f);

    nimcp_brain_destroy(brain_);
    brain_ = nullptr;
    nimcp_ethics_destroy(ethics_);
    ethics_ = nullptr;
    nimcp_knowledge_destroy(knowledge_);
    knowledge_ = nullptr;
}

//=============================================================================
// Sequential create-destroy cycles
//=============================================================================

TEST_F(SRPSplitCrossModuleTest, SequentialCreateDestroyCycles) {
    for (int cycle = 0; cycle < 5; cycle++) {
        char name[32];
        snprintf(name, sizeof(name), "cycle_%d", cycle);

        nimcp_brain_t b = nimcp_brain_create(
            name, NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
        );
        ASSERT_NE(b, nullptr) << "Create failed on cycle " << cycle;

        nimcp_ethics_t e = nimcp_ethics_create();
        ASSERT_NE(e, nullptr) << "Ethics create failed on cycle " << cycle;

        float features[10] = {0.5f};
        nimcp_brain_learn_example(b, features, 10, "class_a", 0.9f);

        float score = -1.0f;
        nimcp_ethics_check(e, features, 10, &score);
        EXPECT_GE(score, 0.0f);

        nimcp_ethics_destroy(e);
        nimcp_brain_destroy(b);
    }
}

//=============================================================================
// Error handling across split modules
//=============================================================================

TEST_F(SRPSplitCrossModuleTest, ErrorHandlingAcrossModules) {
    nimcp_status_t status;

    status = nimcp_brain_learn_example(nullptr, nullptr, 0, nullptr, 0.0f);
    EXPECT_NE(status, NIMCP_SUCCESS);

    status = nimcp_brain_predict(nullptr, nullptr, 0, nullptr, nullptr);
    EXPECT_NE(status, NIMCP_SUCCESS);

    status = nimcp_ethics_check(nullptr, nullptr, 0, nullptr);
    EXPECT_NE(status, NIMCP_SUCCESS);

    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
}

//=============================================================================
// Training pipeline
//=============================================================================

TEST_F(SRPSplitCrossModuleTest, TrainingPipelineAcrossPartFiles) {
    brain_ = nimcp_brain_create(
        "cross_train", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain_, nullptr);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.learning_rate = 0.01f;

    nimcp_status_t status = nimcp_brain_configure_training(brain_, &config);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    float features[10] = {1.0f};
    float targets[3] = {1.0f, 0.0f, 0.0f};
    nimcp_training_result_t result;
    memset(&result, 0, sizeof(result));

    status = nimcp_brain_train_step(brain_, features, 10, targets, 3, &result);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    uint64_t total_steps = 0;
    float total_loss = 0.0f, current_lr = 0.0f;
    status = nimcp_brain_get_training_stats(brain_, &total_steps, &total_loss, &current_lr);
    EXPECT_EQ(status, NIMCP_SUCCESS);
}

//=============================================================================
// COW snapshot
//=============================================================================

TEST_F(SRPSplitCrossModuleTest, COWSnapshotAcrossPartFiles) {
    brain_ = nimcp_brain_create(
        "cross_cow", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain_, nullptr);

    float features[10] = {1.0f};
    nimcp_brain_learn_example(brain_, features, 10, "class_a", 0.9f);

    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain_);
    EXPECT_NE(snapshot, nullptr) << "COW snapshot failed after SRP split";

    if (snapshot) {
        float f2[10] = {0.0f, 1.0f};
        for (int i = 0; i < 10; i++) {
            nimcp_brain_learn_example(brain_, f2, 10, "class_b", 0.8f);
        }

        nimcp_status_t status = nimcp_brain_restore_cow(brain_, snapshot);
        EXPECT_EQ(status, NIMCP_SUCCESS);

        nimcp_brain_snapshot_destroy(snapshot);
    }
}
