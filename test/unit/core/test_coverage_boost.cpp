/**
 * @file test_coverage_boost.cpp
 * @brief Comprehensive test to boost coverage to 95%
 *
 * TARGET: Exercise uncovered code paths in brain.c, neuralnet.c
 */

#include <gtest/gtest.h>
    #include "core/brain/nimcp_brain.h"
    #include "core/neuralnet/nimcp_neuralnet.h"
    #include "utils/memory/nimcp_memory.h"

class CoverageBoostTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        nimcp_memory_cleanup();
    }
};

//=============================================================================
// Brain Coverage Tests
//=============================================================================

TEST_F(CoverageBoostTest, BrainAllTaskTypes) {
    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE,
        BRAIN_TASK_ASSOCIATION,
        BRAIN_TASK_CUSTOM
    };

    for (int i = 0; i < 6; i++) {
        brain_t brain = brain_create("test", BRAIN_SIZE_TINY, tasks[i], 10, 3);
        ASSERT_NE(brain, nullptr);

        float features[10] = {1,2,3,4,5,6,7,8,9,10};

        // Learn
        float loss = brain_learn_example(brain, features, 10, "label", 1.0f);
        EXPECT_GE(loss, 0.0f);

        // Decide
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);

        // Get network
        adaptive_network_t net = brain_get_network(brain);
        EXPECT_NE(net, nullptr);

        // Get neuromodulator system
        neuromodulator_system_t nm = brain_get_neuromodulator_system(brain);
        EXPECT_NE(nm, nullptr);

        brain_destroy(brain);
    }
}

TEST_F(CoverageBoostTest, BrainAllSizePresets) {
    brain_size_t sizes[] = {
        BRAIN_SIZE_TINY,
        BRAIN_SIZE_SMALL,
        BRAIN_SIZE_MEDIUM,
        BRAIN_SIZE_LARGE,
        BRAIN_SIZE_CUSTOM
    };

    for (int i = 0; i < 5; i++) {
        brain_t brain = brain_create("test", sizes[i], BRAIN_TASK_CLASSIFICATION, 8, 2);
        ASSERT_NE(brain, nullptr);
        brain_destroy(brain);
    }
}

TEST_F(CoverageBoostTest, BrainErrorPaths) {
    // NULL checks
    EXPECT_EQ(brain_get_network(nullptr), nullptr);
    EXPECT_EQ(brain_get_neuromodulator_system(nullptr), nullptr);
    EXPECT_EQ(brain_decide(nullptr, nullptr, 0), nullptr);
    EXPECT_EQ(brain_learn_example(nullptr, nullptr, 0, nullptr, 0), -1.0f);

    brain_destroy(nullptr); // Should not crash
    brain_free_decision(nullptr); // Should not crash

    // Error message handling
    const char* error = brain_get_last_error();
    brain_clear_error();
}

TEST_F(CoverageBoostTest, BrainEdgeCases) {
    brain_t brain = brain_create("edge", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[5] = {1, 2, 3, 4, 5};

    // NULL inputs
    EXPECT_EQ(brain_learn_example(brain, nullptr, 5, "label", 1.0f), -1.0f);
    EXPECT_EQ(brain_learn_example(brain, features, 5, nullptr, 1.0f), -1.0f);
    EXPECT_EQ(brain_decide(brain, nullptr, 5), nullptr);

    // Extreme values
    brain_learn_example(brain, features, 5, "label", 1000000.0f); // Very large reward
    brain_learn_example(brain, features, 5, "label", -1.0f); // Negative reward
    brain_learn_example(brain, features, 5, "label", 0.0f); // Zero reward

    // NaN/Infinity
    features[2] = NAN;
    brain_learn_example(brain, features, 5, "label", 1.0f);
    features[2] = INFINITY;
    brain_learn_example(brain, features, 5, "label", 1.0f);

    brain_destroy(brain);
}

//=============================================================================
// NeuralNet Coverage Tests
//=============================================================================

TEST_F(CoverageBoostTest, NeuralNetConfigVariations) {
    network_config_t config1 = {0};
    config1.num_neurons = 100;
    config1.input_size = 10;
    config1.output_size = 5;
    config1.ei_ratio = 0.8f;
    config1.learning_rate = 0.01f;
    config1.target_activity = 0.1f;
    config1.min_weight = -1.0f;
    config1.max_weight = 1.0f;

    network_config_t config2 = {0};
    config2.num_neurons = 200;
    config2.input_size = 20;
    config2.output_size = 10;
    config2.ei_ratio = 0.75f;
    config2.learning_rate = 0.02f;
    config2.target_activity = 0.15f;
    config2.min_weight = -2.0f;
    config2.max_weight = 2.0f;

    network_config_t config3 = {0};
    config3.num_neurons = 50;
    config3.input_size = 5;
    config3.output_size = 2;
    config3.ei_ratio = 0.85f;
    config3.learning_rate = 0.005f;
    config3.target_activity = 0.05f;
    config3.min_weight = -0.5f;
    config3.max_weight = 0.5f;

    network_config_t configs[] = {config1, config2, config3};

    for (int i = 0; i < 3; i++) {
        neural_network_t net = neural_network_create(&configs[i]);
        ASSERT_NE(net, nullptr);
        neural_network_destroy(net);
    }
}

TEST_F(CoverageBoostTest, NeuralNetOperations) {
    network_config_t config = {0};
    config.num_neurons = 100;
    config.input_size = 10;
    config.output_size = 5;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.target_activity = 0.1f;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;

    neural_network_t net = neural_network_create(&config);
    ASSERT_NE(net, nullptr);

    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float output[5];

    // Forward pass
    bool success = neural_network_forward(net, input, 10, output, 5);
    EXPECT_TRUE(success);

    // Multiple iterations
    for (int i = 0; i < 100; i++) {
        neural_network_forward(net, input, 10, output, 5);
    }

    neural_network_destroy(net);
}

TEST_F(CoverageBoostTest, NeuralNetErrorHandling) {
    EXPECT_EQ(neural_network_create(nullptr), nullptr);
    EXPECT_FALSE(neural_network_forward(nullptr, nullptr, 0, nullptr, 0));
    neural_network_destroy(nullptr); // Should not crash
}

//=============================================================================
// Additional Brain Tests
//=============================================================================

TEST_F(CoverageBoostTest, BrainStressTest) {
    brain_t brain = brain_create("stress", BRAIN_SIZE_MEDIUM, BRAIN_TASK_REGRESSION, 50, 10);
    ASSERT_NE(brain, nullptr);

    float features[50];
    for (int i = 0; i < 50; i++) features[i] = (float)i / 50.0f;

    // Intensive training
    for (int iter = 0; iter < 200; iter++) {
        brain_learn_example(brain, features, 50, "target", (float)iter / 100.0f);
    }

    // Multiple predictions
    for (int i = 0; i < 50; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 50);
        if (decision) brain_free_decision(decision);
    }

    brain_destroy(brain);
}

TEST_F(CoverageBoostTest, NeuralNetLayeredConfig) {
    uint32_t layers[3] = {30, 40, 30};

    network_config_t config = {0};
    config.num_neurons = 100;
    config.input_size = 10;
    config.output_size = 5;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.target_activity = 0.1f;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.num_layers = 3;
    config.layer_sizes = layers;

    neural_network_t net = neural_network_create(&config);
    ASSERT_NE(net, nullptr);

    float input[10], output[5];
    for (int i = 0; i < 10; i++) input[i] = (float)i / 10.0f;

    for (int i = 0; i < 100; i++) {
        neural_network_forward(net, input, 10, output, 5);
    }

    neural_network_destroy(net);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(CoverageBoostTest, BrainNeuralNetIntegration) {
    brain_t brain = brain_create("integration", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 20, 5);
    ASSERT_NE(brain, nullptr);

    adaptive_network_t net = brain_get_network(brain);
    ASSERT_NE(net, nullptr);

    float features[20];
    for (int i = 0; i < 20; i++) features[i] = (float)i / 20.0f;

    // Training loop
    for (int epoch = 0; epoch < 10; epoch++) {
        float loss = brain_learn_example(brain, features, 20, "test_label", 1.0f);
        EXPECT_GE(loss, 0.0f);

        brain_decision_t* decision = brain_decide(brain, features, 20);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

TEST_F(CoverageBoostTest, MultipleSimultaneousObjects) {
    // Create multiple brains
    brain_t brains[10];
    for (int i = 0; i < 10; i++) {
        brains[i] = brain_create("multi", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brains[i], nullptr);
    }

    // Use them all intensively
    float features[5] = {1, 2, 3, 4, 5};
    for (int iter = 0; iter < 20; iter++) {
        for (int i = 0; i < 10; i++) {
            brain_learn_example(brains[i], features, 5, "label", 1.0f);
            brain_decision_t* decision = brain_decide(brains[i], features, 5);
            if (decision) brain_free_decision(decision);
        }
    }

    // Cleanup
    for (int i = 0; i < 10; i++) brain_destroy(brains[i]);
}
