/**
 * @file test_srp_split_e2e.cpp
 * @brief End-to-end tests for SRP-split modules
 *
 * Tests complete workflows that exercise multiple split modules
 * in realistic scenarios.
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <cmath>
#include <unistd.h>

class SRPSplitE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_status_t status = nimcp_init();
        ASSERT_EQ(status, NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// Full classification pipeline
//=============================================================================

TEST_F(SRPSplitE2ETest, ClassificationPipeline) {
    nimcp_brain_t brain = nimcp_brain_create(
        "e2e_classify", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 4, 2
    );
    ASSERT_NE(brain, nullptr);

    // Train on pattern
    float p0[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float p1[4] = {1.0f, 1.0f, 0.0f, 0.0f};
    float p2[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    float p3[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    for (int epoch = 0; epoch < 10; epoch++) {
        nimcp_brain_learn_example(brain, p0, 4, "class_0", 0.9f);
        nimcp_brain_learn_example(brain, p1, 4, "class_1", 0.9f);
        nimcp_brain_learn_example(brain, p2, 4, "class_1", 0.9f);
        nimcp_brain_learn_example(brain, p3, 4, "class_0", 0.9f);
    }

    // Predict
    char label[NIMCP_MAX_LABEL_SIZE] = {0};
    float confidence = 0.0f;
    nimcp_status_t status = nimcp_brain_predict(brain, p0, 4, label, &confidence);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    // Accessor from different part file
    uint32_t count = nimcp_brain_get_neuron_count(brain);
    EXPECT_GT(count, 0u);

    // Save/load (IO part files)
    const char* save_path = "/tmp/nimcp_srp_e2e_brain.bin";
    status = nimcp_brain_save(brain, save_path);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_brain_t loaded = nimcp_brain_load(save_path);
    EXPECT_NE(loaded, nullptr);

    if (loaded) {
        char loaded_label[NIMCP_MAX_LABEL_SIZE] = {0};
        float loaded_confidence = 0.0f;
        status = nimcp_brain_predict(loaded, p0, 4, loaded_label, &loaded_confidence);
        EXPECT_EQ(status, NIMCP_SUCCESS);
        nimcp_brain_destroy(loaded);
    }

    nimcp_brain_destroy(brain);
    unlink(save_path);
}

//=============================================================================
// Ethics-gated decision pipeline
//=============================================================================

TEST_F(SRPSplitE2ETest, EthicsGatedDecisionPipeline) {
    nimcp_brain_t brain = nimcp_brain_create(
        "e2e_ethics", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float features[10] = {0.5f, 0.3f, 0.7f, 0.2f, 0.8f,
                          0.1f, 0.9f, 0.4f, 0.6f, 0.5f};
    nimcp_brain_learn_example(brain, features, 10, "action", 0.9f);

    char label[NIMCP_MAX_LABEL_SIZE] = {0};
    float confidence = 0.0f;
    nimcp_status_t status = nimcp_brain_predict(brain, features, 10, label, &confidence);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    float ethics_score = -1.0f;
    status = nimcp_ethics_check(ethics, features, 10, &ethics_score);
    EXPECT_EQ(status, NIMCP_SUCCESS);
    EXPECT_GE(ethics_score, 0.0f);
    EXPECT_LE(ethics_score, 1.0f);

    nimcp_ethics_destroy(ethics);
    nimcp_brain_destroy(brain);
}

//=============================================================================
// Training with stats
//=============================================================================

TEST_F(SRPSplitE2ETest, TrainingWithStats) {
    nimcp_brain_t brain = nimcp_brain_create(
        "e2e_train", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    nimcp_training_config_t config = nimcp_training_config_default();
    config.learning_rate = 0.01f;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    float features[10] = {1.0f};
    float targets[3] = {1.0f, 0.0f, 0.0f};

    for (int step = 0; step < 5; step++) {
        nimcp_training_result_t result;
        memset(&result, 0, sizeof(result));
        status = nimcp_brain_train_step(brain, features, 10, targets, 3, &result);
        EXPECT_EQ(status, NIMCP_SUCCESS) << "Train step " << step << " failed";
    }

    uint64_t total_steps = 0;
    float total_loss = 0.0f, current_lr = 0.0f;
    status = nimcp_brain_get_training_stats(brain, &total_steps, &total_loss, &current_lr);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_brain_destroy(brain);
}

//=============================================================================
// COW snapshot + restore
//=============================================================================

TEST_F(SRPSplitE2ETest, COWSnapshotRestorePipeline) {
    nimcp_brain_t brain = nimcp_brain_create(
        "e2e_cow", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    float f1[10] = {1.0f};
    for (int i = 0; i < 5; i++) {
        nimcp_brain_learn_example(brain, f1, 10, "class_a", 0.9f);
    }

    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Diverge
    float f2[10] = {0.0f, 1.0f};
    for (int i = 0; i < 20; i++) {
        nimcp_brain_learn_example(brain, f2, 10, "class_b", 0.8f);
    }

    // Restore
    nimcp_status_t status = nimcp_brain_restore_cow(brain, snapshot);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_brain_snapshot_destroy(snapshot);
    nimcp_brain_destroy(brain);
}

//=============================================================================
// Full lifecycle across shutdown
//=============================================================================

TEST_F(SRPSplitE2ETest, FullLifecycleAcrossShutdown) {
    const char* save_path = "/tmp/nimcp_srp_e2e_lifecycle.bin";

    {
        nimcp_brain_t brain = nimcp_brain_create(
            "e2e_lifecycle", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
        );
        ASSERT_NE(brain, nullptr);

        float features[10] = {0.5f};
        for (int i = 0; i < 5; i++) {
            nimcp_brain_learn_example(brain, features, 10, "class_a", 0.9f);
        }

        nimcp_status_t status = nimcp_brain_save(brain, save_path);
        EXPECT_EQ(status, NIMCP_SUCCESS);
        nimcp_brain_destroy(brain);
    }

    nimcp_shutdown();
    nimcp_status_t status = nimcp_init();
    ASSERT_EQ(status, NIMCP_SUCCESS);

    {
        nimcp_brain_t loaded = nimcp_brain_load(save_path);
        EXPECT_NE(loaded, nullptr);

        if (loaded) {
            float features[10] = {0.5f};
            char label[NIMCP_MAX_LABEL_SIZE] = {0};
            float confidence = 0.0f;
            status = nimcp_brain_predict(loaded, features, 10, label, &confidence);
            EXPECT_EQ(status, NIMCP_SUCCESS);

            uint32_t count = nimcp_brain_get_neuron_count(loaded);
            EXPECT_GT(count, 0u);
            nimcp_brain_destroy(loaded);
        }
    }

    unlink(save_path);
}

//=============================================================================
// Multi-module workflow
//=============================================================================

TEST_F(SRPSplitE2ETest, MultiModuleWorkflow) {
    nimcp_brain_t brain = nimcp_brain_create(
        "e2e_multi", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    nimcp_ethics_t ethics = nimcp_ethics_create();
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    nimcp_network_t network = nimcp_network_create(10, 5, 8, 0.01f);

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(ethics, nullptr);
    ASSERT_NE(knowledge, nullptr);
    ASSERT_NE(network, nullptr);

    float features[10] = {0.5f, 0.3f, 0.7f, 0.2f, 0.8f,
                          0.1f, 0.9f, 0.4f, 0.6f, 0.5f};

    nimcp_brain_learn_example(brain, features, 10, "class_a", 0.9f);
    char label[NIMCP_MAX_LABEL_SIZE] = {0};
    float confidence = 0.0f;
    nimcp_brain_predict(brain, features, 10, label, &confidence);

    float score = -1.0f;
    nimcp_ethics_check(ethics, features, 10, &score);
    EXPECT_GE(score, 0.0f);

    uint32_t count = nimcp_brain_get_neuron_count(brain);
    nimcp_brain_resize(brain, count + 10);

    nimcp_brain_probe_t probe;
    memset(&probe, 0, sizeof(probe));
    nimcp_brain_probe(brain, &probe);
    EXPECT_GT(probe.num_neurons, 0u);

    nimcp_network_destroy(network);
    nimcp_knowledge_destroy(knowledge);
    nimcp_ethics_destroy(ethics);
    nimcp_brain_destroy(brain);
}
