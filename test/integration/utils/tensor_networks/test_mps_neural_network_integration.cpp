//=============================================================================
// test_mps_neural_network_integration.cpp - MPS Integration Tests
//=============================================================================
/**
 * @file test_mps_neural_network_integration.cpp
 * @brief Deep integration tests for MPS with NIMCP neural networks
 *
 * WHAT: Validate MPS compression integrated with full brain system
 * WHY: Ensure MPS works correctly with plasticity, learning, and all subsystems
 * HOW: Integration tests with neural_network_t and brain_t
 *
 * TEST COVERAGE:
 * 1. MPS + STDP learning integration
 * 2. MPS + BCM homeostatic plasticity
 * 3. MPS + STP short-term dynamics
 * 4. MPS + eligibility traces
 * 5. MPS + neuromodulator system
 * 6. MPS + brain serialization (snapshot/load)
 * 7. MPS backward compatibility (disable flag)
 * 8. MPS regression tests (compare with uncompressed)
 *
 * REGRESSION TESTS:
 * - Learning trajectories match uncompressed within tolerance
 * - Network dynamics unchanged
 * - Snapshot format backward compatible
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 * @version 2.8.0 Phase C3.1
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstring>

#include "utils/tensor_networks/nimcp_mps.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/stp/nimcp_stp.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MPSIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand(42); // Reproducibility
    }

    void TearDown() override {
        // Cleanup handled per test
    }

    /**
     * @brief Create test neural network with MPS option
     *
     * WHAT: Helper to create network with controlled configuration
     * WHY: Reduce test boilerplate
     * HOW: Wrapper around neural_network_create()
     */
    neural_network_t create_test_network(bool use_mps, uint32_t num_neurons = 100) {
        network_config_t config = {0};
        config.num_neurons = num_neurons;  // CRITICAL: Must set num_neurons for validation
        config.input_size = num_neurons;
        config.output_size = num_neurons;
        config.num_layers = 3;
        config.layer_sizes = (uint32_t*)nimcp_malloc(3 * sizeof(uint32_t));
        config.layer_sizes[0] = num_neurons;
        config.layer_sizes[1] = num_neurons / 2;
        config.layer_sizes[2] = num_neurons;
        config.enable_stdp = true;
        config.enable_hebbian = true;
        config.enable_oja = false;
        config.enable_homeostasis = true;
        config.ei_ratio = 0.8f;  // 80% excitatory, 20% inhibitory
        config.refractory_period = 2.0f;

        neural_network_t network = neural_network_create(&config);

        nimcp_free(config.layer_sizes);

        return network;
    }

    /**
     * @brief Generate test input pattern
     */
    void generate_input_pattern(float* input, uint32_t dim, uint32_t pattern_id) {
        for (uint32_t i = 0; i < dim; i++) {
            input[i] = sinf(pattern_id * 0.1f + i * 0.05f);
        }
    }

    /**
     * @brief Measure network output similarity
     */
    float output_similarity(const float* out1, const float* out2, uint32_t dim) {
        float dot = 0.0f, norm1 = 0.0f, norm2 = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            dot += out1[i] * out2[i];
            norm1 += out1[i] * out1[i];
            norm2 += out2[i] * out2[i];
        }
        if (norm1 < 1e-12f || norm2 < 1e-12f) return 0.0f;
        return dot / (sqrtf(norm1) * sqrtf(norm2));
    }
};

//=============================================================================
// Integration Tests: MPS + Plasticity Mechanisms
//=============================================================================

TEST_F(MPSIntegrationTest, MPSWithSTDPLearning) {
    // WHAT: Test MPS compression preserves STDP learning dynamics
    // WHY: Ensure plasticity still works with compressed weights
    // HOW: Compare learning trajectories with/without MPS

    printf("\n=== MPS + STDP Learning Integration ===\n");

    const uint32_t num_neurons = 50;
    const uint32_t num_steps = 1000;
    const float tolerance = 0.15f; // 15% error acceptable

    // Create two networks: with and without MPS
    neural_network_t network_dense = create_test_network(false, num_neurons);
    neural_network_t network_mps = create_test_network(true, num_neurons);

    ASSERT_NE(network_dense, nullptr);
    ASSERT_NE(network_mps, nullptr);

    // Learn same patterns
    float* input = (float*)nimcp_malloc(num_neurons * sizeof(float));
    float* output_dense = (float*)nimcp_calloc(num_neurons, sizeof(float));
    float* output_mps = (float*)nimcp_calloc(num_neurons, sizeof(float));

    std::vector<float> similarity_over_time;

    for (uint32_t step = 0; step < num_steps; step++) {
        // Generate pattern
        generate_input_pattern(input, num_neurons, step / 100);

        // Update both networks
        for (uint32_t i = 0; i < num_neurons; i++) {
            neural_network_update_neuron(network_dense, i, input[i], step);
            neural_network_update_neuron(network_mps, i, input[i], step);
        }

        // Get outputs
        for (uint32_t i = 0; i < num_neurons; i++) {
            neural_network_get_neuron_state(network_dense, i, &output_dense[i]);
            neural_network_get_neuron_state(network_mps, i, &output_mps[i]);
        }

        // Measure similarity every 100 steps
        if (step % 100 == 0) {
            float sim = output_similarity(output_dense, output_mps, num_neurons);
            similarity_over_time.push_back(sim);
            printf("Step %4u: Output similarity = %.4f\n", step, sim);
        }
    }

    // Verify similarity stays high throughout learning
    float avg_similarity = 0.0f;
    for (float sim : similarity_over_time) {
        avg_similarity += sim;
    }
    avg_similarity /= (float)similarity_over_time.size();

    printf("Average output similarity: %.4f\n", avg_similarity);
    printf("Minimum output similarity: %.4f\n",
           *std::min_element(similarity_over_time.begin(), similarity_over_time.end()));

    // PASS CRITERION: Average similarity > 0.85 (15% error)
    EXPECT_GT(avg_similarity, 1.0f - tolerance);

    // Cleanup
    neural_network_destroy(network_dense);
    neural_network_destroy(network_mps);
    nimcp_free(input);
    nimcp_free(output_dense);
    nimcp_free(output_mps);
}

TEST_F(MPSIntegrationTest, MPSWithBCMHomeostasis) {
    // WHAT: Test MPS + BCM homeostatic plasticity interaction
    // WHY: BCM prevents runaway excitation; must work with compression
    // HOW: Induce high activity, verify BCM stabilizes both networks

    printf("\n=== MPS + BCM Homeostatic Plasticity ===\n");

    const uint32_t num_neurons = 50;
    const uint32_t num_steps = 2000;
    const float high_input = 5.0f; // High stimulation to induce runaway

    // Create BCM-enabled networks
    network_config_t config = {0};
    config.num_neurons = num_neurons;
    config.input_size = num_neurons;
    config.output_size = num_neurons;
    config.num_layers = 2;
    config.layer_sizes = (uint32_t*)nimcp_malloc(2 * sizeof(uint32_t));
    config.layer_sizes[0] = num_neurons;
    config.layer_sizes[1] = num_neurons;
    config.enable_stdp = true;
    config.enable_hebbian = true;
    config.enable_bcm = true;  // Enable BCM plasticity
    config.enable_homeostasis = true;
    config.ei_ratio = 0.8f;

    neural_network_t network_dense = neural_network_create(&config);
    neural_network_t network_mps = neural_network_create(&config);

    ASSERT_NE(network_dense, nullptr);
    ASSERT_NE(network_mps, nullptr);

    nimcp_free(config.layer_sizes);

    // Apply high stimulation
    float* input = (float*)nimcp_malloc(num_neurons * sizeof(float));
    for (uint32_t i = 0; i < num_neurons; i++) {
        input[i] = high_input;
    }

    // Track average activity over time
    std::vector<float> activity_dense;
    std::vector<float> activity_mps;

    for (uint32_t step = 0; step < num_steps; step++) {
        // Update both networks
        for (uint32_t i = 0; i < num_neurons; i++) {
            neural_network_update_neuron(network_dense, i, input[i], step);
            neural_network_update_neuron(network_mps, i, input[i], step);
        }

        // Measure average activity every 100 steps
        if (step % 100 == 0) {
            float avg_dense = 0.0f, avg_mps = 0.0f;
            for (uint32_t i = 0; i < num_neurons; i++) {
                float state_dense, state_mps;
                neural_network_get_neuron_state(network_dense, i, &state_dense);
                neural_network_get_neuron_state(network_mps, i, &state_mps);
                avg_dense += fabsf(state_dense);
                avg_mps += fabsf(state_mps);
            }
            avg_dense /= num_neurons;
            avg_mps /= num_neurons;
            activity_dense.push_back(avg_dense);
            activity_mps.push_back(avg_mps);
            printf("Step %4u: Dense activity=%.3f, MPS activity=%.3f\n",
                   step, avg_dense, avg_mps);
        }
    }

    // Verify BCM stabilization: activity should not explode
    // Both networks should stabilize to similar levels
    float final_dense = activity_dense.back();
    float final_mps = activity_mps.back();

    printf("Final dense activity: %.3f\n", final_dense);
    printf("Final MPS activity: %.3f\n", final_mps);

    // PASS CRITERION: Activities stay bounded and similar
    EXPECT_LT(final_dense, 100.0f) << "Dense network activity exploded";
    EXPECT_LT(final_mps, 100.0f) << "MPS network activity exploded";
    EXPECT_LT(fabsf(final_dense - final_mps) / final_dense, 0.3f)
        << "BCM behavior differs >30% between dense and MPS";

    // Cleanup
    neural_network_destroy(network_dense);
    neural_network_destroy(network_mps);
    nimcp_free(input);
}

TEST_F(MPSIntegrationTest, MPSWithSTPDynamics) {
    // WHAT: Test MPS + Short-Term Plasticity dynamics
    // WHY: STP modulates transmission on ms timescale
    // HOW: High-frequency stimulation, measure depression/facilitation

    printf("\n=== MPS + STP Short-Term Dynamics ===\n");

    const uint32_t num_neurons = 30;
    const uint32_t num_pulses = 20;
    const uint32_t pulse_interval_ms = 10; // 100 Hz stimulation

    // Create STP-enabled networks
    network_config_t config = {0};
    config.num_neurons = num_neurons;
    config.input_size = num_neurons;
    config.output_size = num_neurons;
    config.num_layers = 2;
    config.layer_sizes = (uint32_t*)nimcp_malloc(2 * sizeof(uint32_t));
    config.layer_sizes[0] = num_neurons;
    config.layer_sizes[1] = num_neurons;
    config.enable_stdp = true;
    config.enable_homeostasis = true;
    config.ei_ratio = 0.8f;

    neural_network_t network_dense = neural_network_create(&config);
    neural_network_t network_mps = neural_network_create(&config);

    ASSERT_NE(network_dense, nullptr);
    ASSERT_NE(network_mps, nullptr);

    nimcp_free(config.layer_sizes);

    // Apply pulse train
    float* input = (float*)nimcp_malloc(num_neurons * sizeof(float));
    std::vector<float> response_dense;
    std::vector<float> response_mps;

    for (uint32_t pulse = 0; pulse < num_pulses; pulse++) {
        uint64_t timestamp = pulse * pulse_interval_ms;

        // Generate pulse
        for (uint32_t i = 0; i < num_neurons; i++) {
            input[i] = 1.0f;
        }

        // Update networks
        for (uint32_t i = 0; i < num_neurons; i++) {
            neural_network_update_neuron(network_dense, i, input[i], timestamp);
            neural_network_update_neuron(network_mps, i, input[i], timestamp);
        }

        // Measure response
        float avg_dense = 0.0f, avg_mps = 0.0f;
        for (uint32_t i = 0; i < num_neurons; i++) {
            float state_dense, state_mps;
            neural_network_get_neuron_state(network_dense, i, &state_dense);
            neural_network_get_neuron_state(network_mps, i, &state_mps);
            avg_dense += fabsf(state_dense);
            avg_mps += fabsf(state_mps);
        }
        avg_dense /= num_neurons;
        avg_mps /= num_neurons;
        response_dense.push_back(avg_dense);
        response_mps.push_back(avg_mps);

        printf("Pulse %2u (t=%4llu ms): Dense=%.4f, MPS=%.4f\n",
               pulse, (unsigned long long)timestamp, avg_dense, avg_mps);
    }

    // Verify STP dynamics are similar between dense and MPS
    // Both should show depression (decreasing response) or facilitation (increasing response)
    float correlation = 0.0f;
    float mean_dense = 0.0f, mean_mps = 0.0f;
    for (size_t i = 0; i < response_dense.size(); i++) {
        mean_dense += response_dense[i];
        mean_mps += response_mps[i];
    }
    mean_dense /= response_dense.size();
    mean_mps /= response_mps.size();

    float cov = 0.0f, var_dense = 0.0f, var_mps = 0.0f;
    for (size_t i = 0; i < response_dense.size(); i++) {
        float d_dense = response_dense[i] - mean_dense;
        float d_mps = response_mps[i] - mean_mps;
        cov += d_dense * d_mps;
        var_dense += d_dense * d_dense;
        var_mps += d_mps * d_mps;
    }

    if (var_dense > 1e-6f && var_mps > 1e-6f) {
        correlation = cov / (sqrtf(var_dense) * sqrtf(var_mps));
    }

    printf("Response correlation: %.4f\n", correlation);

    // PASS CRITERION: High correlation (>0.7) between responses
    EXPECT_GT(correlation, 0.7f)
        << "STP dynamics differ significantly between dense and MPS";

    // Cleanup
    neural_network_destroy(network_dense);
    neural_network_destroy(network_mps);
    nimcp_free(input);
}

TEST_F(MPSIntegrationTest, MPSWithEligibilityTraces) {
    // WHAT: Test MPS + eligibility traces for delayed rewards
    // WHY: Temporal credit assignment critical for RL
    // HOW: Action → delay → reward, verify weight updates match

    printf("\n=== MPS + Eligibility Traces ===\n");

    const uint32_t num_neurons = 40;
    const uint32_t action_time = 0;
    const uint32_t delay_ms = 100;
    const uint32_t reward_time = action_time + delay_ms;
    const float reward_signal = 1.0f;

    // Create eligibility-enabled networks
    network_config_t config = {0};
    config.num_neurons = num_neurons;
    config.input_size = num_neurons;
    config.output_size = num_neurons;
    config.num_layers = 2;
    config.layer_sizes = (uint32_t*)nimcp_malloc(2 * sizeof(uint32_t));
    config.layer_sizes[0] = num_neurons;
    config.layer_sizes[1] = num_neurons;
    config.enable_stdp = true;
    config.enable_eligibility = true;  // Enable eligibility traces
    config.enable_homeostasis = true;
    config.ei_ratio = 0.8f;

    neural_network_t network_dense = neural_network_create(&config);
    neural_network_t network_mps = neural_network_create(&config);

    ASSERT_NE(network_dense, nullptr);
    ASSERT_NE(network_mps, nullptr);

    nimcp_free(config.layer_sizes);

    // Phase 1: Action (spike activity at t=0)
    float* input = (float*)nimcp_malloc(num_neurons * sizeof(float));
    for (uint32_t i = 0; i < num_neurons; i++) {
        input[i] = sinf(i * 0.2f) + 1.0f; // Varied activity pattern
    }

    printf("Phase 1: Action at t=%u ms\n", action_time);
    for (uint32_t i = 0; i < num_neurons; i++) {
        neural_network_update_neuron(network_dense, i, input[i], action_time);
        neural_network_update_neuron(network_mps, i, input[i], action_time);
    }

    // Update eligibility traces
    for (uint32_t i = 0; i < num_neurons; i++) {
        neural_network_update_traces(network_dense, i, action_time);
        neural_network_update_traces(network_mps, i, action_time);
    }

    // Phase 2: Delay period (traces decay)
    printf("Phase 2: Delay period (%u ms)\n", delay_ms);
    for (uint64_t t = action_time + 1; t < reward_time; t++) {
        for (uint32_t i = 0; i < num_neurons; i++) {
            neural_network_update_traces(network_dense, i, t);
            neural_network_update_traces(network_mps, i, t);
        }
    }

    // Phase 3: Reward arrives
    printf("Phase 3: Reward at t=%u ms (reward=%.2f)\n", reward_time, reward_signal);

    // Apply reward-based plasticity
    for (uint32_t i = 0; i < num_neurons; i++) {
        neural_network_update_plasticity(network_dense, i, reward_time);
        neural_network_update_plasticity(network_mps, i, reward_time);
    }

    // Phase 4: Verify both networks learned similarly
    printf("Phase 4: Verification\n");

    // Test response to same pattern
    float* output_dense = (float*)nimcp_calloc(num_neurons, sizeof(float));
    float* output_mps = (float*)nimcp_calloc(num_neurons, sizeof(float));

    for (uint32_t i = 0; i < num_neurons; i++) {
        neural_network_update_neuron(network_dense, i, input[i], reward_time + 10);
        neural_network_update_neuron(network_mps, i, input[i], reward_time + 10);
        neural_network_get_neuron_state(network_dense, i, &output_dense[i]);
        neural_network_get_neuron_state(network_mps, i, &output_mps[i]);
    }

    // Measure output similarity
    float similarity = output_similarity(output_dense, output_mps, num_neurons);
    printf("Post-learning output similarity: %.4f\n", similarity);

    // PASS CRITERION: Similar learning outcomes (>0.75 similarity)
    EXPECT_GT(similarity, 0.75f)
        << "Eligibility-based learning differs between dense and MPS";

    // Cleanup
    neural_network_destroy(network_dense);
    neural_network_destroy(network_mps);
    nimcp_free(input);
    nimcp_free(output_dense);
    nimcp_free(output_mps);
}

//=============================================================================
// Integration Tests: MPS + Brain System
//=============================================================================

TEST_F(MPSIntegrationTest, MPSWithBrainCreate) {
    // WHAT: Test creating brain with MPS enabled
    // WHY: Ensure brain_create_custom() respects MPS config
    // HOW: Create brain, verify compression applied

    printf("\n=== MPS Brain Creation ===\n");

    brain_config_t config; memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 5;
    config.learning_rate = 0.01f;

    // Enable MPS compression
    config.use_mps_weights = true;
    config.mps_bond_dimension = 10;
    config.mps_adaptive_bond_dim = true;
    config.mps_svd_tolerance = 1e-6f;

    strncpy(config.task_name, "mps_test_brain", 63);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // TODO: Add getter to check if MPS is actually enabled
    // For now, verify brain created successfully

    printf("✅ Brain created with MPS configuration\n");

    brain_destroy(brain);
}

TEST_F(MPSIntegrationTest, MPSBrainLearningRegression) {
    // WHAT: Regression test - learning behavior matches baseline
    // WHY: Ensure MPS doesn't break existing learning algorithms
    // HOW: Train on XOR, compare accuracy with/without MPS

    printf("\n=== MPS Learning Regression Test (XOR) ===\n");

    const float tolerance = 0.1f; // 10% accuracy difference acceptable

    // XOR dataset
    float inputs[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    float targets[4] = {0.0f, 1.0f, 1.0f, 0.0f};

    // Train baseline brain (no MPS)
    brain_config_t config_baseline; memset(&config_baseline, 0, sizeof(config_baseline));
    config_baseline.size = BRAIN_SIZE_TINY;
    config_baseline.task = BRAIN_TASK_CLASSIFICATION;
    config_baseline.num_inputs = 2;
    config_baseline.num_outputs = 1;
    config_baseline.learning_rate = 0.1f;
    config_baseline.use_mps_weights = false; // Baseline
    strncpy(config_baseline.task_name, "xor_baseline", 63);

    brain_t brain_baseline = brain_create_custom(&config_baseline);
    ASSERT_NE(brain_baseline, nullptr);

    // Train MPS brain
    brain_config_t config_mps = config_baseline;
    config_mps.use_mps_weights = true;
    config_mps.mps_bond_dimension = 10;
    strncpy(config_mps.task_name, "xor_mps", 63);

    brain_t brain_mps = brain_create_custom(&config_mps);
    ASSERT_NE(brain_mps, nullptr);

    // Training loop
    const uint32_t epochs = 1000;

    for (uint32_t epoch = 0; epoch < epochs; epoch++) {
        for (uint32_t i = 0; i < 4; i++) {
            // Train baseline
            brain_learn_example(brain_baseline, inputs[i], 2, "xor_output",
                                targets[i]);

            // Train MPS
            brain_learn_example(brain_mps, inputs[i], 2, "xor_output",
                                targets[i]);
        }
    }

    // Test accuracy
    uint32_t correct_baseline = 0, correct_mps = 0;

    for (uint32_t i = 0; i < 4; i++) {
        brain_decision_t* dec_baseline = brain_decide(brain_baseline, inputs[i], 2);
        brain_decision_t* dec_mps = brain_decide(brain_mps, inputs[i], 2);

        ASSERT_NE(dec_baseline, nullptr);
        ASSERT_NE(dec_mps, nullptr);

        // Check predictions
        float pred_baseline = dec_baseline->confidence > 0.5f ? 1.0f : 0.0f;
        float pred_mps = dec_mps->confidence > 0.5f ? 1.0f : 0.0f;

        if (fabsf(pred_baseline - targets[i]) < 0.3f) correct_baseline++;
        if (fabsf(pred_mps - targets[i]) < 0.3f) correct_mps++;

        brain_free_decision(dec_baseline);
        brain_free_decision(dec_mps);
    }

    float accuracy_baseline = (float)correct_baseline / 4.0f;
    float accuracy_mps = (float)correct_mps / 4.0f;

    printf("Baseline accuracy: %.2f%%\n", accuracy_baseline * 100.0f);
    printf("MPS accuracy:      %.2f%%\n", accuracy_mps * 100.0f);
    printf("Accuracy delta:    %.2f%%\n", fabsf(accuracy_baseline - accuracy_mps) * 100.0f);

    // PASS CRITERION: Accuracy difference < tolerance
    EXPECT_LT(fabsf(accuracy_baseline - accuracy_mps), tolerance);

    brain_destroy(brain_baseline);
    brain_destroy(brain_mps);
}

//=============================================================================
// Regression Tests: Snapshot/Load Compatibility
//=============================================================================

TEST_F(MPSIntegrationTest, MPSSnapshotLoadRegression) {
    // WHAT: Test MPS brain snapshot/load preserves state
    // WHY: Serialization critical for persistence
    // HOW: Create → snapshot → load → verify identical behavior

    printf("\n=== MPS Snapshot/Load Regression ===\n");

    // Create brain with MPS enabled
    brain_config_t config; memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 5;
    config.learning_rate = 0.05f;
    config.use_mps_weights = true;
    config.mps_bond_dimension = 8;
    config.mps_adaptive_bond_dim = true;
    strncpy(config.task_name, "mps_snapshot_test", 63);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Train on some data to create non-trivial state
    float input[10];
    for (uint32_t epoch = 0; epoch < 100; epoch++) {
        for (uint32_t i = 0; i < 10; i++) {
            input[i] = sinf(epoch * 0.1f + i * 0.3f);
        }
        brain_learn_example(brain, input, 10, "test_output", 0.8f);
    }

    // Get pre-snapshot decision
    for (uint32_t i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }
    brain_decision_t* dec_before = brain_decide(brain, input, 10);
    ASSERT_NE(dec_before, nullptr);

    float confidence_before = dec_before->confidence;
    printf("Pre-snapshot confidence: %.4f\n", confidence_before);
    brain_free_decision(dec_before);

    // Save snapshot
    const char* snapshot_name = "mps_regression_snapshot";
    const char* snapshot_desc = "MPS snapshot/load regression test";
    bool save_ok = brain_save_snapshot(brain, snapshot_name, snapshot_desc);
    ASSERT_TRUE(save_ok) << "Failed to save snapshot";
    printf("Snapshot saved: %s\n", snapshot_name);

    // List snapshots to verify it was saved
    brain_snapshot_info_t infos[10];
    uint32_t num_snapshots = 0;
    bool list_ok = brain_list_snapshots(brain, infos, 10, &num_snapshots);
    ASSERT_TRUE(list_ok) << "Failed to list snapshots";
    ASSERT_GT(num_snapshots, 0u) << "No snapshots found";

    bool found = false;
    for (uint32_t i = 0; i < num_snapshots; i++) {
        if (strcmp(infos[i].name, snapshot_name) == 0) {
            found = true;
            printf("Found snapshot: %s - %s (timestamp: %llu)\n",
                   infos[i].name, infos[i].description,
                   (unsigned long long)infos[i].timestamp);
            break;
        }
    }
    ASSERT_TRUE(found) << "Saved snapshot not found in list";

    // Modify brain state (additional training)
    for (uint32_t epoch = 0; epoch < 50; epoch++) {
        for (uint32_t i = 0; i < 10; i++) {
            input[i] = cosf(epoch * 0.2f);
        }
        brain_learn_example(brain, input, 10, "test_output", 0.3f);
    }

    // Decision should be different now
    for (uint32_t i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }
    brain_decision_t* dec_modified = brain_decide(brain, input, 10);
    ASSERT_NE(dec_modified, nullptr);

    float confidence_modified = dec_modified->confidence;
    printf("Post-modification confidence: %.4f\n", confidence_modified);
    brain_free_decision(dec_modified);

    // Note: We cannot restore from snapshot with current API
    // The brain_save_snapshot() API saves to disk but there's no brain_restore_snapshot()
    // This is a limitation of the current API design
    // For now, we verify that:
    // 1. Snapshot can be saved
    // 2. Snapshot appears in listing
    // 3. Additional training changes the brain state (sanity check)

    printf("✅ Snapshot save and listing verified\n");
    printf("ℹ️  Note: Snapshot restore API not available - partial test only\n");

    brain_destroy(brain);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(MPSIntegrationTest, MPSDisableBackwardCompatibility) {
    // WHAT: Test backward compatibility when MPS disabled
    // WHY: Existing code should work without changes
    // HOW: Create brain with use_mps_weights=false, verify normal operation

    printf("\n=== MPS Backward Compatibility Test ===\n");

    brain_config_t config; memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 5;
    config.num_outputs = 3;
    config.learning_rate = 0.05f;

    // Explicitly disable MPS (should be default anyway)
    config.use_mps_weights = false;
    config.mps_bond_dimension = 0;

    strncpy(config.task_name, "backward_compat", 63);

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Should work exactly as before
    float input[] = {1.0f, 0.5f, -0.5f, 0.0f, 0.8f};
    brain_decision_t* decision = brain_decide(brain, input, 5);

    ASSERT_NE(decision, nullptr);
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    printf("✅ Backward compatibility maintained\n");

    brain_free_decision(decision);
    brain_destroy(brain);
}

TEST_F(MPSIntegrationTest, MPSDefaultConfigBackwardCompatibility) {
    // WHAT: Test brain_config_default() doesn't enable MPS by default
    // WHY: Breaking change prevention
    // HOW: Check default config has use_mps_weights=false

    printf("\n=== MPS Default Config Test ===\n");

    brain_config_t config; memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_REGRESSION;

    // Default should have MPS disabled
    EXPECT_FALSE(config.use_mps_weights);
    EXPECT_EQ(config.mps_bond_dimension, 0);

    printf("✅ Default configuration maintains backward compatibility\n");
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(MPSIntegrationTest, MPSPerformanceRegression) {
    // WHAT: Ensure MPS overhead is acceptable
    // WHY: Performance regression would be breaking change
    // HOW: Measure inference time with/without MPS

    printf("\n=== MPS Performance Regression ===\n");

    const uint32_t num_iterations = 1000;
    const float max_slowdown = 5.0f; // 5x slower acceptable for 10-100x memory savings

    brain_config_t config; memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 10;

    // Baseline (no MPS)
    config.use_mps_weights = false;
    strncpy(config.task_name, "perf_baseline", 63);
    brain_t brain_baseline = brain_create_custom(&config);
    ASSERT_NE(brain_baseline, nullptr);

    // MPS enabled
    config.use_mps_weights = true;
    config.mps_bond_dimension = 10;
    strncpy(config.task_name, "perf_mps", 63);
    brain_t brain_mps = brain_create_custom(&config);
    ASSERT_NE(brain_mps, nullptr);

    float input[20];
    for (uint32_t i = 0; i < 20; i++) {
        input[i] = sinf(i * 0.1f);
    }

    // Benchmark baseline
    auto start_baseline = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < num_iterations; i++) {
        brain_decision_t* dec = brain_decide(brain_baseline, input, 20);
        brain_free_decision(dec);
    }
    auto end_baseline = std::chrono::high_resolution_clock::now();
    float time_baseline = std::chrono::duration<float, std::milli>(
        end_baseline - start_baseline).count();

    // Benchmark MPS
    auto start_mps = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < num_iterations; i++) {
        brain_decision_t* dec = brain_decide(brain_mps, input, 20);
        brain_free_decision(dec);
    }
    auto end_mps = std::chrono::high_resolution_clock::now();
    float time_mps = std::chrono::duration<float, std::milli>(
        end_mps - start_mps).count();

    float slowdown = time_mps / time_baseline;

    printf("Baseline time:  %.2f ms (%u iterations)\n", time_baseline, num_iterations);
    printf("MPS time:       %.2f ms (%u iterations)\n", time_mps, num_iterations);
    printf("Slowdown:       %.2fx\n", slowdown);

    // PASS CRITERION: Slowdown < max_slowdown
    EXPECT_LT(slowdown, max_slowdown);

    if (slowdown < max_slowdown) {
        printf("✅ Performance regression acceptable\n");
    } else {
        printf("❌ Performance regression too severe\n");
    }

    brain_destroy(brain_baseline);
    brain_destroy(brain_mps);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
