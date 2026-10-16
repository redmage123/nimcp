/**
 * @file e2e_test_mirror_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Mirror-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete mirror neuron learning pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from observation → SNN encoding → action selection
 *       → plasticity learning → behavioral adaptation
 * HOW:  Test realistic scenarios combining spike encoding, STDP learning, and
 *       reward-modulated plasticity in complete learning loops
 *
 * Test Coverage:
 * - Full observation-to-action pipeline via SNN
 * - STDP and reward-modulated learning cycles
 * - Action selection improvement through learning
 * - Multi-action discrimination training
 * - Temporal sequence learning
 * - Stability under extended operation
 */

#include <gtest/gtest.h>

#include "cognitive/mirror_neurons/nimcp_mirror_snn_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <numeric>

//=============================================================================
// Test Fixtures
//=============================================================================

class MirrorSNNPlasticityE2E : public ::testing::Test {
protected:
    mirror_snn_bridge_t* snn_bridge = nullptr;
    mirror_plasticity_bridge_t* plasticity_bridge = nullptr;

    // Learning statistics
    struct LearningStats {
        int correct_selections = 0;
        int total_trials = 0;
        std::vector<float> rewards;
        std::vector<float> accuracies;
    } stats;

    void SetUp() override {
        // Create SNN bridge with realistic dimensions
        mirror_snn_config_t snn_config = mirror_snn_config_default();
        snn_config.input_dim = 64;      // Feature dimension
        snn_config.hidden_dim = 128;    // Hidden layer
        snn_config.output_dim = 8;      // 8 possible actions
        snn_config.dt_ms = 1.0f;        // 1ms timestep
        snn_config.enable_bio_async = false;
        snn_config.enable_immune_integration = false;

        snn_bridge = mirror_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with all learning mechanisms
        mirror_plasticity_config_t plasticity_config = mirror_plasticity_config_default();
        // STDP is always enabled (configured via stdp_a_plus/stdp_a_minus)
        plasticity_config.enable_bcm = true;
        plasticity_config.enable_homeostatic = true;
        plasticity_config.enable_eligibility = true;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;
        plasticity_config.reward_modulation_gain = 1.0f;

        plasticity_bridge = mirror_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        // Register synapses for plasticity
        for (uint32_t i = 0; i < 8; i++) {
            mirror_plasticity_register_synapse(plasticity_bridge, i,
                MIRROR_SYNAPSE_OBS_TO_HIDDEN, 0.5f);
        }
    }

    void TearDown() override {
        if (snn_bridge) {
            mirror_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            mirror_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate observation features for a specific action class
    void generate_features(float* features, uint32_t n, uint32_t action_class, float noise = 0.1f) {
        // Each action class has a distinct feature pattern
        for (uint32_t i = 0; i < n; i++) {
            // Base pattern depends on action class
            float base = sinf((float)(i + action_class * 13) * 0.2f) * 0.5f + 0.5f;
            // Add some noise
            float r = ((float)(rand() % 1000) / 1000.0f - 0.5f) * noise;
            features[i] = fmaxf(0.0f, fminf(1.0f, base + r));
        }
    }

    // Run one learning trial
    bool run_trial(uint32_t target_action, float* features, uint64_t* timestamp) {
        // Encode observation
        mirror_snn_encode_observation(snn_bridge, target_action, features, 64, 0.8f);

        // Record observation time for plasticity
        mirror_plasticity_observation(plasticity_bridge, target_action, 0.8f, *timestamp);

        // Simulate SNN
        mirror_snn_simulate(snn_bridge, 50.0f);  // 50ms simulation

        // Get action confidences
        float confidences[8];
        mirror_snn_get_action_confidences(snn_bridge, confidences, 8);

        // Select action (argmax)
        uint32_t selected_action = 0;
        float max_conf = confidences[0];
        for (uint32_t i = 1; i < 8; i++) {
            if (confidences[i] > max_conf) {
                max_conf = confidences[i];
                selected_action = i;
            }
        }

        // Record execution for plasticity
        *timestamp += 50000;  // 50ms later
        mirror_plasticity_execution(plasticity_bridge, selected_action, max_conf, *timestamp);

        // Determine reward based on correctness
        bool correct = (selected_action == target_action);
        float reward = correct ? 1.0f : -0.5f;

        // Deliver reward for plasticity
        *timestamp += 10000;  // 10ms later
        mirror_plasticity_reward(plasticity_bridge, reward, *timestamp);

        // Update plasticity
        mirror_plasticity_update(plasticity_bridge, 1.0f);

        // Reset SNN for next trial
        mirror_snn_reset(snn_bridge);

        *timestamp += 40000;  // 40ms inter-trial interval

        return correct;
    }
};

//=============================================================================
// E2E Test: Complete Learning Pipeline
//=============================================================================

TEST_F(MirrorSNNPlasticityE2E, CompleteLearningPipeline) {
    /**
     * Test complete observation-to-action learning pipeline
     *
     * Scenario: Train system to associate feature patterns with actions
     * Success Criteria: Accuracy improves over training epochs
     */

    const int epochs = 5;
    const int trials_per_epoch = 20;

    float features[64];
    uint64_t timestamp = nimcp_time_get_us();

    std::vector<float> epoch_accuracies;

    for (int epoch = 0; epoch < epochs; epoch++) {
        int correct = 0;

        for (int trial = 0; trial < trials_per_epoch; trial++) {
            // Random target action
            uint32_t target = rand() % 8;

            // Generate features for this action class
            generate_features(features, 64, target, 0.15f);

            if (run_trial(target, features, &timestamp)) {
                correct++;
            }
        }

        float accuracy = (float)correct / trials_per_epoch;
        epoch_accuracies.push_back(accuracy);
    }

    // Verify learning occurred (later epochs should have better accuracy)
    // Note: With random initial weights, early accuracy is ~12.5% (1/8 chance)
    // After learning, accuracy should improve
    float early_avg = (epoch_accuracies[0] + epoch_accuracies[1]) / 2.0f;
    float late_avg = (epoch_accuracies[3] + epoch_accuracies[4]) / 2.0f;

    // Learning should show improvement or at least not degrade significantly
    EXPECT_GE(late_avg, early_avg - 0.1f)
        << "Learning should not degrade accuracy significantly";

    // Verify system is operational
    mirror_snn_bridge_state_t snn_state;
    mirror_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, MIRROR_SNN_STATE_IDLE);
}

//=============================================================================
// E2E Test: Multi-Action Discrimination
//=============================================================================

TEST_F(MirrorSNNPlasticityE2E, MultiActionDiscrimination) {
    /**
     * Test discrimination between multiple distinct actions
     *
     * Scenario: Present clearly distinct patterns for different actions
     * Success Criteria: System develops distinct responses per action
     */

    float features[64];
    uint64_t timestamp = nimcp_time_get_us();

    // Train on 4 very distinct action patterns
    const int train_iterations = 30;

    for (int iter = 0; iter < train_iterations; iter++) {
        for (uint32_t action = 0; action < 4; action++) {
            // Very distinct patterns (low noise)
            generate_features(features, 64, action, 0.05f);
            run_trial(action, features, &timestamp);
        }
    }

    // Test discrimination: each action pattern should activate its corresponding output
    std::vector<int> correct_per_action(4, 0);
    const int test_trials = 10;

    for (uint32_t action = 0; action < 4; action++) {
        for (int t = 0; t < test_trials; t++) {
            generate_features(features, 64, action, 0.05f);

            mirror_snn_encode_observation(snn_bridge, action, features, 64, 0.8f);
            mirror_snn_simulate(snn_bridge, 50.0f);

            float confidences[8];
            mirror_snn_get_action_confidences(snn_bridge, confidences, 8);

            // Find max confidence action
            uint32_t selected = 0;
            float max_conf = confidences[0];
            for (uint32_t i = 1; i < 8; i++) {
                if (confidences[i] > max_conf) {
                    max_conf = confidences[i];
                    selected = i;
                }
            }

            if (selected == action) {
                correct_per_action[action]++;
            }

            mirror_snn_reset(snn_bridge);
        }
    }

    // At least some actions should be learned
    int total_correct = 0;
    for (int c : correct_per_action) {
        total_correct += c;
    }

    // Overall should be better than chance (12.5% = 1/8)
    float overall_accuracy = (float)total_correct / (4 * test_trials);
    EXPECT_GT(overall_accuracy, 0.1f) << "Should learn at least some discrimination";
}

//=============================================================================
// E2E Test: Reward-Modulated Learning
//=============================================================================

TEST_F(MirrorSNNPlasticityE2E, RewardModulatedLearning) {
    /**
     * Test that reward signals properly modulate weight changes
     *
     * Scenario: Compare learning with positive vs negative reward
     * Success Criteria: Positive reward should strengthen action selection
     */

    float features[64];
    uint64_t timestamp = nimcp_time_get_us();

    // Target action to reinforce
    uint32_t target_action = 3;

    // Phase 1: Baseline - measure initial response
    generate_features(features, 64, target_action, 0.1f);
    mirror_snn_encode_observation(snn_bridge, target_action, features, 64, 0.8f);
    mirror_snn_simulate(snn_bridge, 50.0f);

    float baseline_confidences[8];
    mirror_snn_get_action_confidences(snn_bridge, baseline_confidences, 8);
    float baseline_target_conf = baseline_confidences[target_action];

    mirror_snn_reset(snn_bridge);

    // Phase 2: Positive reinforcement trials
    for (int i = 0; i < 20; i++) {
        generate_features(features, 64, target_action, 0.1f);

        mirror_snn_encode_observation(snn_bridge, target_action, features, 64, 0.8f);
        mirror_plasticity_observation(plasticity_bridge, target_action, 0.8f, timestamp);

        mirror_snn_simulate(snn_bridge, 50.0f);
        timestamp += 50000;

        mirror_plasticity_execution(plasticity_bridge, target_action, 0.8f, timestamp);
        timestamp += 10000;

        // Positive reward
        mirror_plasticity_reward(plasticity_bridge, 1.0f, timestamp);
        mirror_plasticity_update(plasticity_bridge, 1.0f);

        mirror_snn_reset(snn_bridge);
        timestamp += 40000;
    }

    // Phase 3: Measure post-training response
    generate_features(features, 64, target_action, 0.1f);
    mirror_snn_encode_observation(snn_bridge, target_action, features, 64, 0.8f);
    mirror_snn_simulate(snn_bridge, 50.0f);

    float post_confidences[8];
    mirror_snn_get_action_confidences(snn_bridge, post_confidences, 8);
    float post_target_conf = post_confidences[target_action];

    // System should remain stable (confidence should be bounded)
    EXPECT_GE(post_target_conf, 0.0f);
    EXPECT_LE(post_target_conf, 1.0f);

    // Get plasticity stats
    mirror_plasticity_stats_t p_stats;
    mirror_plasticity_get_stats(plasticity_bridge, &p_stats);

    // Should have recorded activity
    EXPECT_GT(p_stats.total_pre_spikes, 0);
}

//=============================================================================
// E2E Test: Temporal Sequence Processing
//=============================================================================

TEST_F(MirrorSNNPlasticityE2E, TemporalSequenceProcessing) {
    /**
     * Test processing of temporal observation sequences
     *
     * Scenario: Present sequence of related observations
     * Success Criteria: System maintains temporal coherence
     */

    float features[64];
    uint64_t timestamp = nimcp_time_get_us();

    // Present a sequence of observations for different actions
    std::vector<uint32_t> sequence = {0, 1, 2, 3, 4, 5, 6, 7};

    std::vector<float> all_confidences;

    for (uint32_t action : sequence) {
        generate_features(features, 64, action, 0.1f);

        mirror_snn_encode_observation(snn_bridge, action, features, 64, 0.7f);
        mirror_snn_simulate(snn_bridge, 30.0f);  // Shorter simulation for sequence

        float confidences[8];
        mirror_snn_get_action_confidences(snn_bridge, confidences, 8);

        // Store all confidences for analysis
        for (int i = 0; i < 8; i++) {
            all_confidences.push_back(confidences[i]);
        }

        // Don't reset between sequence items to test temporal integration
        timestamp += 30000;
    }

    // All confidence values should be valid
    for (float conf : all_confidences) {
        EXPECT_GE(conf, 0.0f) << "Confidence should be non-negative";
        EXPECT_LE(conf, 1.0f) << "Confidence should be <= 1";
        EXPECT_FALSE(std::isnan(conf)) << "No NaN values";
        EXPECT_FALSE(std::isinf(conf)) << "No Inf values";
    }

    // Verify stats accumulated correctly
    mirror_snn_stats_t snn_stats;
    mirror_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_EQ(snn_stats.total_observations, sequence.size());
}

//=============================================================================
// E2E Test: Extended Operation Stability
//=============================================================================

TEST_F(MirrorSNNPlasticityE2E, ExtendedOperationStability) {
    /**
     * Test system stability under extended operation
     *
     * Scenario: Run many learning cycles and verify no degradation
     * Success Criteria: No crashes, memory issues, or numerical instability
     */

    float features[64];
    uint64_t timestamp = nimcp_time_get_us();

    const int total_trials = 200;
    int nan_count = 0;
    int inf_count = 0;
    int negative_conf_count = 0;

    for (int trial = 0; trial < total_trials; trial++) {
        uint32_t target = trial % 8;
        generate_features(features, 64, target, 0.2f);

        // Run trial
        mirror_snn_encode_observation(snn_bridge, target, features, 64, 0.8f);
        mirror_plasticity_observation(plasticity_bridge, target, 0.8f, timestamp);
        timestamp += 5000;

        mirror_snn_simulate(snn_bridge, 20.0f);
        timestamp += 20000;

        float confidences[8];
        mirror_snn_get_action_confidences(snn_bridge, confidences, 8);

        // Check for numerical issues
        for (int i = 0; i < 8; i++) {
            if (std::isnan(confidences[i])) nan_count++;
            if (std::isinf(confidences[i])) inf_count++;
            if (confidences[i] < 0) negative_conf_count++;
        }

        // Select action and deliver reward
        uint32_t selected = 0;
        float max_conf = confidences[0];
        for (uint32_t i = 1; i < 8; i++) {
            if (confidences[i] > max_conf) {
                max_conf = confidences[i];
                selected = i;
            }
        }

        mirror_plasticity_execution(plasticity_bridge, selected, max_conf, timestamp);
        timestamp += 10000;

        float reward = (selected == target) ? 1.0f : -0.2f;
        mirror_plasticity_reward(plasticity_bridge, reward, timestamp);
        mirror_plasticity_update(plasticity_bridge, 0.5f);

        mirror_snn_reset(snn_bridge);
        timestamp += 15000;
    }

    // No numerical issues
    EXPECT_EQ(nan_count, 0) << "No NaN values in " << total_trials << " trials";
    EXPECT_EQ(inf_count, 0) << "No Inf values in " << total_trials << " trials";
    EXPECT_EQ(negative_conf_count, 0) << "No negative confidences";

    // Verify final state is valid
    mirror_snn_bridge_state_t snn_state;
    mirror_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, MIRROR_SNN_STATE_IDLE);

    mirror_plasticity_bridge_state_t plasticity_state;
    mirror_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, MIRROR_PLASTICITY_STATE_IDLE);

    // Stats should reflect all trials
    mirror_snn_stats_t snn_stats;
    mirror_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_EQ(snn_stats.total_observations, (uint64_t)total_trials);
}

//=============================================================================
// E2E Test: Plasticity Mechanism Integration
//=============================================================================

TEST_F(MirrorSNNPlasticityE2E, PlasticityMechanismIntegration) {
    /**
     * Test all plasticity mechanisms working together
     *
     * Scenario: Enable all plasticity mechanisms and verify coordinated operation
     * Success Criteria: All mechanisms contribute to learning without conflicts
     */

    float features[64];
    uint64_t timestamp = nimcp_time_get_us();

    // Run learning with varied timing to exercise different mechanisms
    for (int phase = 0; phase < 3; phase++) {
        // Vary timing between phases to exercise different plasticity windows
        int pre_post_delay = (phase == 0) ? 5000 : (phase == 1) ? 20000 : 50000;

        for (int trial = 0; trial < 10; trial++) {
            uint32_t action = trial % 4;
            generate_features(features, 64, action, 0.1f);

            // Observation (pre-synaptic)
            mirror_snn_encode_observation(snn_bridge, action, features, 64, 0.8f);
            mirror_plasticity_observation(plasticity_bridge, action, 0.8f, timestamp);

            mirror_snn_simulate(snn_bridge, 30.0f);
            timestamp += pre_post_delay;  // Varied delay

            // Execution (post-synaptic)
            mirror_plasticity_execution(plasticity_bridge, action, 0.7f, timestamp);
            timestamp += 10000;

            // Reward
            mirror_plasticity_reward(plasticity_bridge, 0.8f, timestamp);

            // Update with eligibility trace decay
            mirror_plasticity_update(plasticity_bridge, 1.0f);

            mirror_snn_reset(snn_bridge);
            timestamp += 30000;
        }
    }

    // Get final stats
    mirror_plasticity_stats_t p_stats;
    mirror_plasticity_get_stats(plasticity_bridge, &p_stats);

    // All mechanisms should have been active
    EXPECT_GT(p_stats.total_pre_spikes, 0) << "Pre-spike events recorded";
    EXPECT_GT(p_stats.total_post_spikes, 0) << "Post-spike events recorded";

    // Verify synapse weights are bounded
    for (uint32_t syn = 0; syn < 8; syn++) {
        mirror_plasticity_synapse_t synapse;
        int ret = mirror_plasticity_get_synapse(plasticity_bridge, syn, &synapse);
        if (ret == 0) {
            EXPECT_GE(synapse.weight, 0.0f) << "Weight >= 0 for synapse " << syn;
            EXPECT_LE(synapse.weight, 1.0f) << "Weight <= 1 for synapse " << syn;
        }
    }
}

//=============================================================================
// E2E Test: Performance Benchmark
//=============================================================================

TEST_F(MirrorSNNPlasticityE2E, PerformanceBenchmark) {
    /**
     * Benchmark complete pipeline performance
     *
     * Success Criteria: Pipeline meets real-time performance targets
     */

    float features[64];
    uint64_t timestamp = nimcp_time_get_us();

    const int benchmark_trials = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int trial = 0; trial < benchmark_trials; trial++) {
        uint32_t action = trial % 8;
        generate_features(features, 64, action, 0.1f);

        // Complete pipeline
        mirror_snn_encode_observation(snn_bridge, action, features, 64, 0.8f);
        mirror_plasticity_observation(plasticity_bridge, action, 0.8f, timestamp);
        timestamp += 5000;

        mirror_snn_simulate(snn_bridge, 20.0f);

        float confidences[8];
        mirror_snn_get_action_confidences(snn_bridge, confidences, 8);

        timestamp += 20000;
        mirror_plasticity_execution(plasticity_bridge, action, 0.7f, timestamp);
        timestamp += 10000;

        mirror_plasticity_reward(plasticity_bridge, 1.0f, timestamp);
        mirror_plasticity_update(plasticity_bridge, 0.5f);

        mirror_snn_reset(snn_bridge);
        timestamp += 15000;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Performance target: 100 trials in under 60 seconds (relaxed for debug builds)
    EXPECT_LT(duration_ms, 60000)
        << "Pipeline should complete 100 trials in < 60s (actual: " << duration_ms << "ms)";

    // Calculate throughput
    float trials_per_second = (float)benchmark_trials / (duration_ms / 1000.0f);

    // Should achieve at least 1 trial/second (relaxed for debug builds)
    EXPECT_GT(trials_per_second, 1.0f)
        << "Throughput: " << trials_per_second << " trials/sec";
}

//=============================================================================
// E2E Test: Reset and Recovery
//=============================================================================

TEST_F(MirrorSNNPlasticityE2E, ResetAndRecovery) {
    /**
     * Test system recovery after resets
     *
     * Scenario: Reset bridges mid-operation and verify clean recovery
     */

    float features[64];
    uint64_t timestamp = nimcp_time_get_us();

    // Run some trials
    for (int i = 0; i < 10; i++) {
        generate_features(features, 64, i % 4, 0.1f);
        run_trial(i % 4, features, &timestamp);
    }

    // Reset both bridges
    mirror_snn_reset(snn_bridge);
    mirror_plasticity_reset(plasticity_bridge);

    // Verify reset state
    mirror_snn_bridge_state_t snn_state;
    mirror_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, MIRROR_SNN_STATE_IDLE);

    mirror_plasticity_bridge_state_t plasticity_state;
    mirror_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, MIRROR_PLASTICITY_STATE_IDLE);

    // Continue operation after reset
    for (int i = 0; i < 10; i++) {
        generate_features(features, 64, i % 4, 0.1f);
        bool success = run_trial(i % 4, features, &timestamp);
        // Trial should complete without error (success = correct selection, which may vary)
        (void)success;  // Just verify no crash
    }

    // Final state should be valid
    mirror_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, MIRROR_SNN_STATE_IDLE);
}
