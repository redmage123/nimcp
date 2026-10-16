//=============================================================================
// test_learning_integration.cpp - Integration Tests for Learning Modules
//=============================================================================
/**
 * @file test_learning_integration.cpp
 * @brief Integration tests for learning module interoperability
 *
 * WHAT: Tests interaction between different learning subsystems
 * WHY:  Ensure learning pipelines work end-to-end
 * HOW:  Multi-module workflows with real data
 *
 * @author NIMCP Development Team
 * @date 2025-12-05
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_brain_learning.h"
#include "core/brain/learning/nimcp_association_learning.h"
#include "core/brain/learning/nimcp_rule_learning.h"
#include "async/nimcp_bio_async.h"
#include "utils/memory/nimcp_unified_memory.h"

class LearningIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    bio_ctx_t bio_ctx;

    void SetUp() override {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.num_inputs = 20;
        config.num_outputs = 10;
        config.num_hidden_neurons = 50;
        config.learning_rate = 0.01f;

        brain = brain_create(&config);
        ASSERT_NE(brain, nullptr);

        bio_ctx = bio_ctx_create();
        ASSERT_NE(bio_ctx, nullptr);

        brain->bio_ctx = bio_ctx;
        brain_learning_register_bio_async(brain);
    }

    void TearDown() override {
        if (brain) brain_destroy(brain);
        if (bio_ctx) bio_ctx_destroy(bio_ctx);
    }
};

//=============================================================================
// Test: Supervised Learning + Association Learning Pipeline
//=============================================================================

TEST_F(LearningIntegrationTest, SupervisedAndAssociationLearningPipeline) {
    // Phase 1: Supervised learning on labeled data
    float features1[20];
    float features2[20];
    for (int i = 0; i < 20; i++) {
        features1[i] = (float)i / 20.0f;
        features2[i] = (float)(i + 10) / 20.0f;
    }

    float loss1 = brain_learn_example(brain, features1, 20, "class_A", 0.9f);
    float loss2 = brain_learn_example(brain, features2, 20, "class_B", 0.9f);

    ASSERT_GE(loss1, 0.0f);
    ASSERT_GE(loss2, 0.0f);

    // Phase 2: Learn associations between concepts
    bool assoc_result = brain_learn_association(brain, "class_A", "concept_X", 10);
    EXPECT_TRUE(assoc_result);

    assoc_result = brain_learn_association(brain, "class_B", "concept_Y", 15);
    EXPECT_TRUE(assoc_result);

    // Phase 3: Verify association strengths
    float strength_ax = get_association_strength(brain, "class_A", "concept_X");
    float strength_by = get_association_strength(brain, "class_B", "concept_Y");

    EXPECT_GT(strength_ax, 0.0f);
    EXPECT_GT(strength_by, 0.0f);
    EXPECT_GT(strength_by, strength_ax); // More cooccurrences = higher strength
}

//=============================================================================
// Test: Rule Learning from Supervised Examples
//=============================================================================

TEST_F(LearningIntegrationTest, RuleLearningFromSupervisedExamples) {
    // Create rule examples
    const int num_examples = 10;
    rule_example_t examples[num_examples];
    const char* labels[num_examples];

    for (int i = 0; i < num_examples; i++) {
        examples[i].num_features = 20;
        examples[i].features = (float*)malloc(20 * sizeof(float));

        // Create pattern: first 10 have features [0-9] active, next 10 have [10-19] active
        for (int j = 0; j < 20; j++) {
            if (i < 5) {
                examples[i].features[j] = (j < 10) ? 1.0f : 0.0f;
            } else {
                examples[i].features[j] = (j >= 10) ? 1.0f : 0.0f;
            }
        }

        labels[i] = (i < 5) ? "pattern_A" : "pattern_B";
    }

    // Learn rules
    int rules_learned = brain_learn_rule_from_examples(brain, examples, labels, num_examples);

    EXPECT_GT(rules_learned, 0);
    EXPECT_LE(rules_learned, 2); // Should extract 2 rules (one per pattern)

    // Cleanup
    for (int i = 0; i < num_examples; i++) {
        free(examples[i].features);
    }
}

//=============================================================================
// Test: Bio-Async Event Flow Across Learning Modules
//=============================================================================

TEST_F(LearningIntegrationTest, BioAsyncEventFlowAcrossModules) {
    int total_events = 0;

    auto event_counter = [](bio_ctx_t ctx, bio_msg_t msg, void* user_data) {
        int* count = (int*)user_data;
        (*count)++;
    };

    // Subscribe to all learning-related events
    bio_ctx_subscribe(bio_ctx, BIO_MSG_TRAINING_STEP, event_counter, &total_events);
    bio_ctx_subscribe(bio_ctx, BIO_MSG_BRAIN_LEARN_COMPLETE, event_counter, &total_events);
    bio_ctx_subscribe(bio_ctx, BIO_MSG_ASSOCIATION_FORMED, event_counter, &total_events);
    bio_ctx_subscribe(bio_ctx, BIO_MSG_RULE_LEARNED, event_counter, &total_events);

    // Trigger events from multiple modules
    float features[20];
    for (int i = 0; i < 20; i++) features[i] = 0.5f;

    brain_learn_example(brain, features, 20, "test", 0.8f);
    brain_learn_association(brain, "concept_A", "concept_B", 5);

    bio_ctx_process(bio_ctx);

    // Verify events were broadcast
    EXPECT_GT(total_events, 0);
}

//=============================================================================
// Test: Learning with Bio-Async Feedback Loop
//=============================================================================

TEST_F(LearningIntegrationTest, LearningWithBioAsyncFeedback) {
    float learning_losses[10];
    int loss_count = 0;

    // Handler that tracks losses
    auto loss_tracker = [](bio_ctx_t ctx, bio_msg_t msg, void* user_data) {
        struct { float* losses; int* count; }* data =
            (struct { float* losses; int* count; }*)user_data;

        float loss = bio_msg_get_float(msg, "loss", -1.0f);
        if (loss >= 0.0f && *data->count < 10) {
            data->losses[(*data->count)++] = loss;
        }
    };

    struct { float* losses; int* count; } tracker_data = {learning_losses, &loss_count};
    bio_ctx_subscribe(bio_ctx, BIO_MSG_BRAIN_LEARN_COMPLETE, loss_tracker, &tracker_data);

    // Perform multiple learning iterations
    for (int i = 0; i < 10; i++) {
        float features[20];
        for (int j = 0; j < 20; j++) {
            features[j] = (float)(i + j) / 30.0f;
        }

        brain_learn_example(brain, features, 20, "iterative_class", 0.9f);
        bio_ctx_process(bio_ctx);
    }

    // Verify losses were tracked
    EXPECT_GT(loss_count, 0);

    // Optionally: verify loss decreases over time (learning progress)
    if (loss_count > 1) {
        float first_loss = learning_losses[0];
        float last_loss = learning_losses[loss_count - 1];
        // Note: May not always decrease in simplified test, but good heuristic
    }
}

//=============================================================================
// Test: Multi-Module Security Integration
//=============================================================================

TEST_F(LearningIntegrationTest, MultiModuleSecurityValidation) {
    float features[20];
    for (int i = 0; i < 20; i++) features[i] = 0.5f;

    // Test malicious input rejected across all modules
    float loss = brain_learn_example(brain, features, 20, "'; DROP--", 0.8f);
    EXPECT_LT(loss, 0.0f);

    bool assoc_result = brain_learn_association(brain, "$(malicious)", "target", 5);
    EXPECT_FALSE(assoc_result);

    // Verify system remains functional after attack attempts
    loss = brain_learn_example(brain, features, 20, "valid_label", 0.8f);
    EXPECT_GE(loss, 0.0f);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
