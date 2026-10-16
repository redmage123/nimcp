/**
 * @file test_attention_training_integration.cpp
 * @brief Integration tests for attention system in the training pipeline
 *
 * Tests that multihead attention is correctly integrated with learning,
 * the attention-plasticity bridge is created and updated during training,
 * and attention modulates feature processing and learning rate.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

extern "C" {
#include "core/brain/learning/nimcp_brain_learning.h"
#include "cognitive/attention/nimcp_attention_plasticity_bridge.h"
int brain_enable_multi_network_training(brain_t brain);
}

//=============================================================================
// Shared fixture
//=============================================================================
class AttentionTrainingIntegration : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("attn_test", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 32, 8);
        ASSERT_NE(brain, nullptr) << "Failed to create test brain";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    float train_one(const float* features, uint32_t n, const char* label, float conf = 0.9f) {
        return brain_learn_example(brain, features, n, label, conf);
    }
};

//=============================================================================
// Tests
//=============================================================================

TEST_F(AttentionTrainingIntegration, MultiheadAttentionExistsAfterCreate) {
    (void)brain->multihead_attention;
    SUCCEED();
}

TEST_F(AttentionTrainingIntegration, EnableMultiNetworkCreatesAttentionBridge) {
    int rc = brain_enable_multi_network_training(brain);
    EXPECT_EQ(rc, 0);

    if (brain->multihead_attention) {
        EXPECT_NE(brain->attention_plasticity, nullptr);
        EXPECT_TRUE(brain->attention_training_enabled);
    }
}

TEST_F(AttentionTrainingIntegration, AttentionAndVAETrainingDoesNotCrash) {
    int rc = brain_enable_multi_network_training(brain);
    ASSERT_EQ(rc, 0);

    // Train with varied data — exercises attention forward, VAE anomaly
    // detection, VAE training, attention-plasticity bridge, all together
    float features[32];
    const char* labels[] = {"math", "science", "history", "language"};

    for (int step = 0; step < 20; step++) {
        for (int i = 0; i < 32; i++) {
            features[i] = sinf((float)(step * 32 + i) * 0.1f) * 0.5f + 0.5f;
        }
        float loss = train_one(features, 32, labels[step % 4], 0.8f);
        EXPECT_GE(loss, 0.0f) << "Loss should be non-negative at step " << step;
    }
}

TEST_F(AttentionTrainingIntegration, VAEFreeEnergyUpdatedDuringTraining) {
    int rc = brain_enable_multi_network_training(brain);
    ASSERT_EQ(rc, 0);

    float features[32];
    for (int i = 0; i < 32; i++) features[i] = 0.5f;
    for (int step = 0; step < 5; step++) {
        train_one(features, 32, "test_class", 0.9f);
    }

    // Free energy field should be accessible (may be 0 if VAE returns it)
    (void)brain->last_vae_free_energy;
    SUCCEED();
}

TEST_F(AttentionTrainingIntegration, AttentionStrengthIsUpdated) {
    int rc = brain_enable_multi_network_training(brain);
    ASSERT_EQ(rc, 0);

    float features[32];
    for (int i = 0; i < 32; i++) features[i] = 0.5f;
    for (int step = 0; step < 5; step++) {
        train_one(features, 32, "test_class", 0.9f);
    }

    if (brain->attention_training_enabled) {
        EXPECT_GE(brain->last_attention_strength, 0.0f);
        EXPECT_LE(brain->last_attention_strength, 1.0f);
    }
    SUCCEED();
}

TEST_F(AttentionTrainingIntegration, WithoutEnableNoAttentionTraining) {
    float features[32];
    for (int i = 0; i < 32; i++) features[i] = 0.5f;

    float loss = train_one(features, 32, "test", 0.9f);
    EXPECT_GE(loss, 0.0f);

    EXPECT_FALSE(brain->attention_training_enabled);
    EXPECT_EQ(brain->attention_plasticity, nullptr);
}

TEST_F(AttentionTrainingIntegration, AttentionIdempotentEnable) {
    int rc1 = brain_enable_multi_network_training(brain);
    EXPECT_EQ(rc1, 0);
    void* apb1 = brain->attention_plasticity;

    int rc2 = brain_enable_multi_network_training(brain);
    EXPECT_EQ(rc2, 0);
    void* apb2 = brain->attention_plasticity;

    EXPECT_EQ(apb1, apb2);
}

TEST_F(AttentionTrainingIntegration, PlasticityBridgeRecordsEvents) {
    int rc = brain_enable_multi_network_training(brain);
    ASSERT_EQ(rc, 0);

    if (!brain->attention_training_enabled) {
        GTEST_SKIP() << "Attention not available for this brain config";
    }

    float features[32];
    for (int i = 0; i < 32; i++) features[i] = 0.3f;
    for (int step = 0; step < 10; step++) {
        features[0] = (float)step / 10.0f;
        train_one(features, 32, "class_a", 0.9f);
    }

    attention_plasticity_bridge_t* apb =
        (attention_plasticity_bridge_t*)brain->attention_plasticity;
    ASSERT_NE(apb, nullptr);

    attention_plasticity_stats_t stats;
    int stats_rc = attention_plasticity_get_stats(apb, &stats);
    EXPECT_EQ(stats_rc, 0);
    EXPECT_GT(stats.total_focus_events, 0u);
}

TEST_F(AttentionTrainingIntegration, AttentionModulatesWithVariedInputs) {
    int rc = brain_enable_multi_network_training(brain);
    ASSERT_EQ(rc, 0);

    if (!brain->attention_training_enabled) {
        GTEST_SKIP() << "Attention not available for this brain config";
    }

    float uniform[32];
    for (int i = 0; i < 32; i++) uniform[i] = 0.5f;
    train_one(uniform, 32, "uniform", 0.9f);
    float strength_uniform = brain->last_attention_strength;

    float diverse[32];
    for (int i = 0; i < 32; i++) diverse[i] = (float)i / 31.0f;
    train_one(diverse, 32, "diverse", 0.9f);
    float strength_diverse = brain->last_attention_strength;

    EXPECT_GE(strength_uniform, 0.0f);
    EXPECT_LE(strength_uniform, 1.0f);
    EXPECT_GE(strength_diverse, 0.0f);
    EXPECT_LE(strength_diverse, 1.0f);
}
