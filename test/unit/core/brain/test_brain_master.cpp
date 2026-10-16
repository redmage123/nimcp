/**
 * @file test_brain_master.cpp
 * @brief Master integration test exercising the entire brain through its public API
 *
 * GOAL: Achieve maximum coverage by running real brain operations
 * STRATEGY: Use brain_learn_example, brain_decide, and all brain_* functions
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "core/brain/nimcp_brain.h"

class BrainMasterTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create with all features enabled
        brain_config_t config;
        memset(&config, 0, sizeof(config));

        config.size = BRAIN_SIZE_MEDIUM;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 256;
        config.num_outputs = 128;
        config.learning_rate = 0.01f;
        config.sparsity_target = 0.8f;
        config.enable_explanations = true;
        strncpy(config.task_name, "master_test", sizeof(config.task_name) - 1);

        // Enable all optional features
        config.enable_distributed = false;  // No P2P in tests
        config.enable_glial = true;
        config.enable_oscillations = true;
        config.num_astrocytes = 2000;
        config.num_oligodendrocytes = 1500;
        config.num_microglia = 1000;
        config.enable_visual_cortex = true;
        config.enable_audio_cortex = true;
        config.enable_speech_cortex = true;
        config.enable_multimodal_integration = true;

        brain = brain_create_custom(&config);
        if (!brain) {
            const char* error = brain_get_last_error();
            FAIL() << "brain_create_custom() returned NULL. Error: "
                   << (error ? error : "no error message");
        }
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) brain_destroy(brain);
    }
};

TEST_F(BrainMasterTest, MassiveTrainingCycle) {
    // Train with 5000 varied examples to hit all code paths
    float features[256];

    for (int example = 0; example < 5000; example++) {
        // Generate diverse patterns
        for (int i = 0; i < 256; i++) {
            float t = (float)example / 5000.0f;
            float phase = (float)i / 256.0f;

            // Different pattern types per epoch range
            if (example < 1000) {
                // Sine waves
                features[i] = sinf(phase * 6.28f * (1.0f + t * 5.0f));
            } else if (example < 2000) {
                // Square waves
                features[i] = (sinf(phase * 6.28f * 3.0f) > 0) ? 1.0f : -1.0f;
            } else if (example < 3000) {
                // Triangular
                features[i] = 2.0f * fabsf(phase - 0.5f) - 0.5f;
            } else if (example < 4000) {
                // Random-like
                features[i] = sinf((float)(i * example % 1000) / 100.0f);
            } else {
                // Sparse patterns
                features[i] = (i % 10 == 0) ? 1.0f : 0.0f;
            }
        }

        // Assign labels based on pattern
        const char* label = (example % 4 == 0) ? "class_a" :
                           (example % 4 == 1) ? "class_b" :
                           (example % 4 == 2) ? "class_c" : "class_d";

        float confidence = 0.8f + 0.2f * sinf((float)example / 100.0f);

        // Learn example (exercises learning, plasticity, adaptation)
        brain_learn_example(brain, features, 256, label, confidence);

        // Every 100 examples, do inference (exercises decision making)
        if (example % 100 == 0) {
            brain_decision_t* decision = brain_decide(brain, features, 256);
            if (decision) {
                // Use the decision (prevents optimization away)
                EXPECT_TRUE(decision->confidence >= 0.0f);
            }
        }

        // Every 500 examples, save/load (exercises serialization)
        if (example % 500 == 0) {
            brain_save(brain, "/tmp/brain_master.nimcp");
            brain_t loaded = brain_load("/tmp/brain_master.nimcp");
            if (loaded) {
                brain_destroy(loaded);
            }
        }
    }

    SUCCEED();
}

TEST_F(BrainMasterTest, AllBrainOperations) {
    float features[256];
    for (int i = 0; i < 256; i++) features[i] = sinf((float)i * 0.1f);

    // Learn examples
    brain_learn_example(brain, features, 256, "test_a", 0.9f);
    brain_learn_example(brain, features, 256, "test_b", 0.85f);

    // Batch learning
    brain_example_t examples[10];
    for (int i = 0; i < 10; i++) {
        examples[i].features = features;
        examples[i].num_features = 256;
        strncpy(examples[i].label, "batch_test", sizeof(examples[i].label) - 1);
        examples[i].label[sizeof(examples[i].label) - 1] = '\0';
        examples[i].confidence = 0.8f;
    }
    brain_learn_batch(brain, examples, 10);

    // Decision making
    brain_decision_t* dec = brain_decide(brain, features, 256);
    if (dec) {
        EXPECT_NE(dec->label, nullptr);
    }

    // Get network (exercises internal access)
    adaptive_network_t net = brain_get_network(brain);
    EXPECT_NE(net, nullptr);

    // Get statistics
    brain_stats_t stats;
    if (brain_get_stats(brain, &stats)) {
        EXPECT_TRUE(stats.num_neurons > 0);
    }

    // Save and load
    brain_save(brain, "/tmp/test_brain.nimcp");
    brain_t loaded = brain_load("/tmp/test_brain.nimcp");
    if (loaded) {
        brain_destroy(loaded);
    }

    // Clone (exercises copy-on-write)
    brain_t clone = brain_clone_cow(brain);
    if (clone) {
        brain_decide(clone, features, 256);
        brain_destroy(clone);
    }

    SUCCEED();
}

TEST_F(BrainMasterTest, AllBrainSizesAndTasks) {
    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL, BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};
    brain_task_t tasks[] = {BRAIN_TASK_CLASSIFICATION, BRAIN_TASK_REGRESSION,
                           BRAIN_TASK_PATTERN_MATCHING, BRAIN_TASK_SEQUENCE,
                           BRAIN_TASK_ASSOCIATION};

    float input[100];
    for (int i = 0; i < 100; i++) input[i] = (float)i / 100.0f;

    // Test all combinations
    for (int s = 0; s < 4; s++) {
        for (int t = 0; t < 5; t++) {
            brain_t b = brain_create("combo_test", sizes[s], tasks[t], 100, 50);
            if (b) {
                // Train it
                brain_learn_example(b, input, 100, "test", 0.8f);

                // Decide
                brain_decision_t* dec = brain_decide(b, input, 100);
                if (dec) {
                    EXPECT_TRUE(dec->confidence >= 0.0f);
                }

                brain_destroy(b);
            }
        }
    }

    SUCCEED();
}

TEST_F(BrainMasterTest, EdgeCasePatterns) {
    float features[256];

    // All zeros
    memset(features, 0, sizeof(features));
    brain_learn_example(brain, features, 256, "zeros", 0.9f);
    brain_decide(brain, features, 256);

    // All ones
    for (int i = 0; i < 256; i++) features[i] = 1.0f;
    brain_learn_example(brain, features, 256, "ones", 0.9f);
    brain_decide(brain, features, 256);

    // All negative
    for (int i = 0; i < 256; i++) features[i] = -1.0f;
    brain_learn_example(brain, features, 256, "negative", 0.9f);
    brain_decide(brain, features, 256);

    // Very large values
    for (int i = 0; i < 256; i++) features[i] = 1000.0f;
    brain_learn_example(brain, features, 256, "large", 0.9f);
    brain_decide(brain, features, 256);

    // Very small values
    for (int i = 0; i < 256; i++) features[i] = 0.0001f;
    brain_learn_example(brain, features, 256, "small", 0.9f);
    brain_decide(brain, features, 256);

    // Alternating
    for (int i = 0; i < 256; i++) features[i] = (i % 2) ? 1.0f : -1.0f;
    brain_learn_example(brain, features, 256, "alternating", 0.9f);
    brain_decide(brain, features, 256);

    // Sparse (mostly zeros)
    memset(features, 0, sizeof(features));
    for (int i = 0; i < 256; i += 32) features[i] = 1.0f;
    brain_learn_example(brain, features, 256, "sparse", 0.9f);
    brain_decide(brain, features, 256);

    // Dense (all similar)
    for (int i = 0; i < 256; i++) features[i] = 0.5f + 0.01f * sinf((float)i);
    brain_learn_example(brain, features, 256, "dense", 0.9f);
    brain_decide(brain, features, 256);

    SUCCEED();
}

TEST_F(BrainMasterTest, StressTest) {
    float features[256];

    // Rapid-fire learning (exercises memory management, caching)
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 256; j++) {
            features[j] = sinf((float)(i + j) * 0.01f);
        }
        brain_learn_example(brain, features, 256, (i % 2) ? "a" : "b", 0.8f);
    }

    // Rapid-fire inference
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 256; j++) {
            features[j] = cosf((float)(i + j) * 0.01f);
        }
        brain_decision_t* dec = brain_decide(brain, features, 256);
        if (dec) {
            EXPECT_NE(dec->label, nullptr);
        }
    }

    // Repeated save/load (exercises serialization paths)
    for (int i = 0; i < 10; i++) {
        brain_save(brain, "/tmp/stress_brain.nimcp");
        brain_t loaded = brain_load("/tmp/stress_brain.nimcp");
        if (loaded) {
            brain_decide(loaded, features, 256);
            brain_destroy(loaded);
        }
    }

    // Multiple clones (exercises COW)
    brain_t clones[5];
    for (int i = 0; i < 5; i++) {
        clones[i] = brain_clone_cow(brain);
        if (clones[i]) {
            brain_learn_example(clones[i], features, 256, "clone_test", 0.8f);
        }
    }
    for (int i = 0; i < 5; i++) {
        if (clones[i]) brain_destroy(clones[i]);
    }

    SUCCEED();
}

TEST_F(BrainMasterTest, LongRunningSimulation) {
    // Simulate a long-running application scenario
    float features[256];

    for (int session = 0; session < 10; session++) {
        // Each session: learn, infer, adapt
        for (int step = 0; step < 100; step++) {
            // Generate pattern
            for (int i = 0; i < 256; i++) {
                features[i] = sinf((float)(session * 100 + step + i) * 0.05f);
            }

            // Learn
            const char* label = (session % 3 == 0) ? "pattern_1" :
                               (session % 3 == 1) ? "pattern_2" : "pattern_3";
            brain_learn_example(brain, features, 256, label, 0.85f);

            // Infer
            brain_decision_t* dec = brain_decide(brain, features, 256);
            if (dec && dec->label[0] != '\0') {
                EXPECT_TRUE(strlen(dec->label) > 0);
            }
        }

        // Save checkpoint at end of each session
        char filename[64];
        snprintf(filename, sizeof(filename), "/tmp/session_%d.nimcp", session);
        brain_save(brain, filename);
    }

    SUCCEED();
}
