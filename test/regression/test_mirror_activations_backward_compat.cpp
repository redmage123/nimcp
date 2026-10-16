/**
 * @file test_mirror_activations_backward_compat.cpp
 * @brief Regression tests for brain mirror activation API backward compatibility
 *
 * WHAT: Ensure new mirror neuron APIs don't break existing functionality
 * WHY:  Maintain backward compatibility with existing brain API users
 * HOW:  Test that brains without mirror neurons still work correctly
 *
 * REGRESSION SCENARIOS:
 * - Brains created before mirror neuron feature
 * - Brains with mirror neurons disabled
 * - Mixed configurations (some features on, mirror neurons off)
 * - Existing brain_observe_action() behavior preserved
 * - No performance regression in non-mirror-neuron paths
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cstring>
#include "core/brain/nimcp_brain.h"

class MirrorActivationsBackwardCompatTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Clean up any brains created during tests
    }
};

//=============================================================================
// TEST SUITE 1: Legacy Brain Configurations
//=============================================================================

TEST_F(MirrorActivationsBackwardCompatTest, Legacy_BrainWithoutMirrorNeurons_WorksNormally) {
    // Create brain with mirror neurons disabled (legacy config)
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "legacy_brain", 63);
    config.enable_mirror_neurons = false;
    config.enable_theory_of_mind = false;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Standard brain operations should work
    float input[128] = {0}; // Brain expects 128 inputs
    for (int i = 0; i < 10 && i < 128; i++) {
        input[i] = 0.1f * (i + 1);
    }

    brain_decision_t* decision = brain_decide(brain, input, 128);
    EXPECT_NE(decision, nullptr);

    if (decision) {
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

TEST_F(MirrorActivationsBackwardCompatTest, Legacy_GetMirrorActivations_FailsGracefully) {
    // Create brain without mirror neurons
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "no_mirror", 63);
    config.enable_mirror_neurons = false;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Getting mirror activations should fail gracefully
    float activations[100];
    uint32_t out_size = 0;

    EXPECT_FALSE(brain_get_mirror_activations(brain, activations, 100, &out_size));
    EXPECT_EQ(out_size, 0); // Should set output to 0 on error

    brain_destroy(brain);
}

TEST_F(MirrorActivationsBackwardCompatTest, Legacy_ComputeEmpathy_FailsGracefully) {
    // Create brain without mirror neurons
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "no_empathy", 63);
    config.enable_mirror_neurons = false;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Computing empathy should fail gracefully
    float features[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float valence, arousal, confidence;

    EXPECT_FALSE(brain_compute_empathy(brain, features, 5,
                                      &valence, &arousal, &confidence));

    brain_destroy(brain);
}

//=============================================================================
// TEST SUITE 2: Mixed Configuration Compatibility
//=============================================================================

TEST_F(MirrorActivationsBackwardCompatTest, MixedConfig_ToMWithoutMirrorNeurons) {
    // Configuration where ToM is enabled but mirror neurons are not
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "tom_only", 63);
    config.enable_theory_of_mind = true;
    config.enable_mirror_neurons = false;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Brain should still create successfully
    // Mirror neuron APIs should fail gracefully
    float activations[100];
    uint32_t out_size = 0;

    EXPECT_FALSE(brain_get_mirror_activations(brain, activations, 100, &out_size));

    brain_destroy(brain);
}

TEST_F(MirrorActivationsBackwardCompatTest, MixedConfig_MirrorNeuronsWithoutToM) {
    // Configuration where mirror neurons are enabled but ToM is not
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "mirror_only", 63);
    config.enable_mirror_neurons = true;
    config.enable_theory_of_mind = false;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Mirror neuron APIs should work
    float activations[100];
    uint32_t out_size = 0;

    EXPECT_TRUE(brain_get_mirror_activations(brain, activations, 100, &out_size));

    brain_destroy(brain);
}

//=============================================================================
// TEST SUITE 3: Existing brain_observe_action() Behavior Preserved
//=============================================================================

TEST_F(MirrorActivationsBackwardCompatTest, ObserveAction_WithMirrorNeurons_WorksAsExpected) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "mirror_brain", 63);
    config.enable_mirror_neurons = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // brain_observe_action should work (existing API)
    float action[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    EXPECT_TRUE(brain_observe_action(brain, action, 5, 1));

    brain_destroy(brain);
}

TEST_F(MirrorActivationsBackwardCompatTest, ObserveAction_WithoutMirrorNeurons_ReturnsTrue) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "no_mirror", 63);
    config.enable_mirror_neurons = false;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // brain_observe_action should still return true (no-op is valid)
    float action[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    EXPECT_TRUE(brain_observe_action(brain, action, 5, 1));

    brain_destroy(brain);
}

//=============================================================================
// TEST SUITE 4: Performance Regression Tests
//=============================================================================

TEST_F(MirrorActivationsBackwardCompatTest, Performance_NoOverheadWhenDisabled) {
    // Create two brains: one with mirror neurons, one without
    brain_config_t config_no_mirror = {};
    config_no_mirror.size = BRAIN_SIZE_SMALL;
    config_no_mirror.task = BRAIN_TASK_CLASSIFICATION;
    config_no_mirror.num_inputs = 128;
    config_no_mirror.num_outputs = 20;
    strncpy(config_no_mirror.task_name, "no_mirror", 63);
    config_no_mirror.enable_mirror_neurons = false;

    brain_config_t config_with_mirror = {};
    config_with_mirror.size = BRAIN_SIZE_SMALL;
    config_with_mirror.task = BRAIN_TASK_CLASSIFICATION;
    config_with_mirror.num_inputs = 128;
    config_with_mirror.num_outputs = 20;
    strncpy(config_with_mirror.task_name, "with_mirror", 63);
    config_with_mirror.enable_mirror_neurons = true;

    brain_t brain_no_mirror = brain_create_custom(&config_no_mirror);
    brain_t brain_with_mirror = brain_create_custom(&config_with_mirror);

    ASSERT_NE(brain_no_mirror, nullptr);
    ASSERT_NE(brain_with_mirror, nullptr);

    // Time brain_decide() on both
    float input[128] = {0}; // Brain expects 128 inputs
    for (int i = 0; i < 10 && i < 128; i++) {
        input[i] = 0.1f * (i + 1);
    }

    // Warm up
    for (int i = 0; i < 10; i++) {
        brain_decision_t* d1 = brain_decide(brain_no_mirror, input, 128);
        brain_decision_t* d2 = brain_decide(brain_with_mirror, input, 128);
        if (d1) brain_free_decision(d1);
        if (d2) brain_free_decision(d2);
    }

    // Benchmark without mirror neurons
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        brain_decision_t* decision = brain_decide(brain_no_mirror, input, 128);
        if (decision) brain_free_decision(decision);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    // Benchmark with mirror neurons (but not observing, just regular decide)
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        brain_decision_t* decision = brain_decide(brain_with_mirror, input, 128);
        if (decision) brain_free_decision(decision);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Having mirror neurons enabled shouldn't add >50% overhead to regular decide
    // (Some overhead is acceptable for additional features)
    EXPECT_LT(duration2.count(), duration1.count() * 1.5)
        << "Mirror neurons should not add excessive overhead to brain_decide()";

    brain_destroy(brain_no_mirror);
    brain_destroy(brain_with_mirror);
}

TEST_F(MirrorActivationsBackwardCompatTest, Performance_MirrorAPIsFastEnough) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "perf_test", 63);
    config.enable_mirror_neurons = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Set up some mirror neuron activity
    float action[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    brain_observe_action(brain, action, 5, 1);

    // Test brain_get_mirror_activations performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        float activations[100];
        uint32_t out_size = 0;
        brain_get_mirror_activations(brain, activations, 100, &out_size);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be < 100 microseconds per call on average
    EXPECT_LT(duration.count() / 10000, 100)
        << "brain_get_mirror_activations should be fast";

    // Test brain_compute_empathy performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        float valence, arousal, confidence;
        brain_compute_empathy(brain, action, 5, &valence, &arousal, &confidence);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be < 1000 microseconds (1ms) per call on average
    EXPECT_LT(duration.count() / 1000, 1000)
        << "brain_compute_empathy should be fast enough for real-time use";

    brain_destroy(brain);
}

//=============================================================================
// TEST SUITE 5: API Contract Stability
//=============================================================================

TEST_F(MirrorActivationsBackwardCompatTest, Contract_NullHandlingConsistent) {
    // All APIs should handle NULL consistently
    float activations[100];
    uint32_t out_size = 0;
    float features[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float valence, arousal, confidence;

    // All should return false for NULL brain
    EXPECT_FALSE(brain_get_mirror_activations(nullptr, activations, 100, &out_size));
    EXPECT_FALSE(brain_compute_empathy(nullptr, features, 5,
                                      &valence, &arousal, &confidence));
}

TEST_F(MirrorActivationsBackwardCompatTest, Contract_ErrorStatesClearable) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "error_test", 63);
    config.enable_mirror_neurons = false;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Trigger error
    float activations[100];
    uint32_t out_size = 0;
    EXPECT_FALSE(brain_get_mirror_activations(brain, activations, 100, &out_size));

    // Should still be able to use brain for other operations
    float input[128] = {0}; // Brain expects 128 inputs
    for (int i = 0; i < 5 && i < 128; i++) {
        input[i] = 0.1f * (i + 1);
    }
    brain_decision_t* decision = brain_decide(brain, input, 128);

    // Should work despite previous error
    EXPECT_NE(decision, nullptr);

    if (decision) brain_free_decision(decision);
    brain_destroy(brain);
}

//=============================================================================
// TEST SUITE 6: Serialization Compatibility
//=============================================================================

TEST_F(MirrorActivationsBackwardCompatTest, Serialization_BrainWithMirrorNeurons_SaveLoad) {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 128;
    config.num_outputs = 20;
    strncpy(config.task_name, "serial_test", 63);
    config.enable_mirror_neurons = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Train some observations
    float action[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    brain_observe_action(brain, action, 5, 1);

    // Save
    const char* filepath = "/tmp/test_mirror_brain.nimcp";
    EXPECT_TRUE(brain_save(brain, filepath));

    // Load
    brain_t loaded_brain = brain_load(filepath);
    ASSERT_NE(loaded_brain, nullptr);

    // Should have same capabilities
    float activations[100];
    uint32_t out_size = 0;
    EXPECT_TRUE(brain_get_mirror_activations(loaded_brain, activations, 100, &out_size));

    brain_destroy(brain);
    brain_destroy(loaded_brain);
    unlink(filepath);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
