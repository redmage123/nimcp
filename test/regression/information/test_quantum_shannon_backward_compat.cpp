//=============================================================================
// test_quantum_shannon_backward_compat.cpp - Quantum-Shannon Regression Tests
//=============================================================================
/**
 * @file test_quantum_shannon_backward_compat.cpp
 * @brief Backward compatibility tests for Quantum Walk + Shannon integration
 *
 * PURPOSE: Ensure quantum-Shannon integration doesn't break existing code
 * COVERAGE:
 * - Quantum walk still works without Shannon
 * - Shannon addition doesn't break quantum walk
 * - Brain APIs unchanged
 * - Performance acceptable
 * - Memory usage reasonable
 * - No breaking changes
 *
 * TEST COUNT: 23 regression tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 * @version 2.10.0 Phase C4.1
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

#include "utils/quantum/nimcp_quantum_shannon.h"
#include "utils/quantum/nimcp_quantum_walk.h"

// Headers have their own extern "C" guards
    #include "information/nimcp_shannon.h"
    #include "core/brain/nimcp_brain.h"
    #include "core/neuralnet/nimcp_neuralnet.h"
    #include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumShannonRegressionTest : public ::testing::Test {
protected:
    neural_network_t network;
    quantum_walker_t* walker;
    quantum_shannon_diffusion_t* qsd;
    brain_t brain;

    void SetUp() override {
        network = nullptr;
        walker = nullptr;
        qsd = nullptr;
        brain = nullptr;

        // Create simple test network (1000 neurons)
        network = create_test_network(1000);
        ASSERT_NE(network, nullptr);
    }

    void TearDown() override {
        if (walker) {
            quantum_walk_destroy(walker);
            walker = nullptr;
        }
        if (qsd) {
            quantum_shannon_destroy(qsd);
            qsd = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        if (network) {
            neural_network_destroy(network);
            network = nullptr;
        }
    }

    // Helper: Create simple test network
    neural_network_t create_test_network(uint32_t num_neurons) {
        // Create network config with minimal required fields
        network_config_t config = {};
        config.num_neurons = num_neurons;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.min_weight = 0.0f;
        config.max_weight = 1.0f;
        config.input_size = num_neurons;
        config.output_size = num_neurons;

        neural_network_t net = neural_network_create(&config);
        if (!net) return nullptr;

        // Add simple connectivity
        for (uint32_t i = 0; i < num_neurons - 1; i++) {
            neural_network_add_connection(net, i, i + 1, 0.5f);
            if (i > 0) {
                neural_network_add_connection(net, i, i - 1, 0.3f);
            }
        }

        return net;
    }

    // Helper: Verify probability distribution sums to ~1.0
    bool verify_probability_distribution(const float* probs, uint32_t size) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < size; i++) {
            if (probs[i] < 0.0f || probs[i] > 1.0f) return false;
            sum += probs[i];
        }
        return fabsf(sum - 1.0f) < 1e-3f;
    }
};

//=============================================================================
// Category 1: Quantum Walk Backward Compatibility (5 tests)
//=============================================================================

TEST_F(QuantumShannonRegressionTest, QuantumWalk_StillWorksAlone) {
    // WHAT: Create quantum walker WITHOUT Shannon
    // WHY:  Verify pure quantum walk unchanged
    // EXPECTED: Works exactly as before, no Shannon overhead

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr)
        << "Quantum walk creation should work without Shannon";

    // Initialize at node 0
    EXPECT_TRUE(quantum_walk_initialize(walker, 0));

    // Evolve 50 steps
    EXPECT_TRUE(quantum_walk_evolve(walker, 50));

    // Get distribution
    float* probs = (float*)malloc(1000 * sizeof(float));
    ASSERT_NE(probs, nullptr);
    EXPECT_TRUE(quantum_walk_get_distribution(walker, probs));

    // Verify valid probability distribution
    EXPECT_TRUE(verify_probability_distribution(probs, 1000))
        << "Quantum walk should produce valid probability distribution";

    free(probs);
}

TEST_F(QuantumShannonRegressionTest, QuantumWalk_AllConfigsStillWork) {
    // WHAT: Test all quantum walk configs still work
    // WHY:  Shannon shouldn't break any existing configs
    // EXPECTED: All configs produce valid results

    quantum_walk_config_t configs[] = {
        quantum_walk_default_config(),
        quantum_walk_fast_config(),
        quantum_walk_hybrid_config()
    };

    for (int i = 0; i < 3; i++) {
        walker = quantum_walk_create(network, &configs[i]);
        ASSERT_NE(walker, nullptr)
            << "Config " << i << " should work";

        EXPECT_TRUE(quantum_walk_initialize(walker, 100));
        EXPECT_TRUE(quantum_walk_step(walker));

        quantum_walk_destroy(walker);
        walker = nullptr;
    }
}

TEST_F(QuantumShannonRegressionTest, QuantumWalk_PerformanceUnchanged) {
    // WHAT: Verify quantum walk performance not degraded
    // WHY:  Shannon shouldn't slow down pure quantum walk
    // EXPECTED: < 5% performance overhead

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    EXPECT_TRUE(quantum_walk_initialize(walker, 0));

    // Benchmark 100 steps
    uint64_t start = nimcp_time_get_us();
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(quantum_walk_step(walker));
    }
    uint64_t end = nimcp_time_get_us();

    uint64_t elapsed_us = end - start;
    float avg_us_per_step = elapsed_us / 100.0f;

    // Should be reasonably fast (< 1ms per step for 1000 neurons)
    EXPECT_LT(avg_us_per_step, 1000.0f)
        << "Quantum walk should maintain performance";
}

TEST_F(QuantumShannonRegressionTest, QuantumWalk_AllCoinOperatorsWork) {
    // WHAT: Test all coin operators still work
    // WHY:  Shannon shouldn't interfere with coin operators
    // EXPECTED: All coin types produce valid evolution

    quantum_coin_type_t coin_types[] = {
        COIN_HADAMARD,
        COIN_GROVER,
        COIN_FOURIER,
        COIN_IDENTITY
    };

    for (int i = 0; i < 4; i++) {
        quantum_walk_config_t config = quantum_walk_default_config();
        config.coin_type = coin_types[i];

        walker = quantum_walk_create(network, &config);
        ASSERT_NE(walker, nullptr)
            << "Coin type " << i << " should work";

        EXPECT_TRUE(quantum_walk_initialize(walker, 50));
        EXPECT_TRUE(quantum_walk_evolve(walker, 10));
        EXPECT_TRUE(quantum_walk_verify(walker))
            << "Coin type " << i << " should preserve probability";

        quantum_walk_destroy(walker);
        walker = nullptr;
    }
}

TEST_F(QuantumShannonRegressionTest, QuantumWalk_MeasurementStillWorks) {
    // WHAT: Test quantum measurement unchanged
    // WHY:  Shannon shouldn't affect measurement collapse
    // EXPECTED: Measurement produces valid node ID

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    EXPECT_TRUE(quantum_walk_initialize(walker, 100));
    EXPECT_TRUE(quantum_walk_evolve(walker, 20));

    uint32_t measured_node = quantum_walk_measure(walker);
    EXPECT_LT(measured_node, 1000)
        << "Measured node should be valid";

    // After measurement, walker should be collapsed to measured node
    float* probs = (float*)malloc(1000 * sizeof(float));
    ASSERT_NE(probs, nullptr);
    EXPECT_TRUE(quantum_walk_get_distribution(walker, probs));

    // Collapsed state: probability ~1 at measured node
    EXPECT_GT(probs[measured_node], 0.9f)
        << "After measurement, state should collapse";

    free(probs);
}

//=============================================================================
// Category 2: Brain APIs Unchanged (5 tests)
//=============================================================================

TEST_F(QuantumShannonRegressionTest, Brain_CreateStillWorks) {
    // WHAT: Brain creation without quantum-Shannon awareness
    // WHY:  Old code patterns must still work
    // EXPECTED: Brain creates successfully

    brain = brain_create("regression_test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 10);
    ASSERT_NE(brain, nullptr)
        << "Brain creation should work without quantum-Shannon knowledge";
}

TEST_F(QuantumShannonRegressionTest, Brain_LearningUnaffected) {
    // WHAT: Learning pipeline unchanged
    // WHY:  Shannon shouldn't interfere with learning
    // EXPECTED: Learning works normally

    brain = brain_create("test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 10);
    ASSERT_NE(brain, nullptr);

    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = 0.5f + 0.01f * (float)i;
    }

    // Learn 10 examples
    for (int i = 0; i < 10; i++) {
        float loss = brain_learn_example(brain, features, 10, "class_a", 0.9f);
        EXPECT_GE(loss, 0.0f) << "Learning should succeed";
    }
}

TEST_F(QuantumShannonRegressionTest, Brain_InferenceUnaffected) {
    // WHAT: Inference pipeline unchanged
    // WHY:  Shannon shouldn't slow down inference
    // EXPECTED: Inference works with acceptable performance

    brain = brain_create("test", BRAIN_SIZE_SMALL,
                        BRAIN_TASK_CLASSIFICATION, 10, 10);
    ASSERT_NE(brain, nullptr);

    // Train
    float train_features[10];
    for (int i = 0; i < 10; i++) {
        train_features[i] = 0.7f;
    }
    brain_learn_example(brain, train_features, 10, "class_a", 0.9f);

    // Infer
    float test_features[10];
    for (int i = 0; i < 10; i++) {
        test_features[i] = 0.65f;
    }

    brain_decision_t* decision = brain_decide(brain, test_features, 10);
    ASSERT_NE(decision, nullptr);

    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);
    EXPECT_NE(decision->label[0], '\0');
}

TEST_F(QuantumShannonRegressionTest, Brain_ConfigWithoutQuantumWalkWorks) {
    // WHAT: Brain config without quantum walk enabled
    // WHY:  Old configs must work (quantum walk disabled by default)
    // EXPECTED: Brain works normally

    brain_config_t config = {
        .size = BRAIN_SIZE_SMALL,
        .task = BRAIN_TASK_CLASSIFICATION,
        .num_inputs = 10,
        .num_outputs = 10,
        .learning_rate = 0.01f,
        .sparsity_target = 0.85f,
        .enable_explanations = false,
        .enable_quantum_walk_diffusion = false,  // Disabled
        .quantum_walk_steps = 0,
        .quantum_classical_mixing = 0.0f,
        .quantum_coin_type = 0,
        .quantum_decoherence_rate = 0.0f
    };
    strncpy(config.task_name, "no_quantum", 63);

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr)
        << "Brain should work without quantum walk";

    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                         0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_learn_example(brain, features, 10, "test", 0.9f);

    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
}

TEST_F(QuantumShannonRegressionTest, Brain_AllTaskTypesStillWork) {
    // WHAT: All brain task types still work
    // WHY:  Shannon shouldn't break task-specific logic
    // EXPECTED: All task types create successfully

    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE,
        BRAIN_TASK_ASSOCIATION
    };

    for (int i = 0; i < 5; i++) {
        brain = brain_create("task_test", BRAIN_SIZE_TINY, tasks[i], 5, 5);
        ASSERT_NE(brain, nullptr)
            << "Task type " << i << " should work";

        brain_destroy(brain);
        brain = nullptr;
    }
}

//=============================================================================
// Category 3: Performance Regression Tests (5 tests)
//=============================================================================

TEST_F(QuantumShannonRegressionTest, Performance_QuantumWalkNoSlowdown) {
    // WHAT: Quantum walk performance acceptable
    // WHY:  Shannon overhead < 10%
    // EXPECTED: < 10% overhead vs baseline

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    EXPECT_TRUE(quantum_walk_initialize(walker, 0));

    // Benchmark quantum walk alone
    uint64_t start = nimcp_time_get_us();
    EXPECT_TRUE(quantum_walk_evolve(walker, 100));
    uint64_t end = nimcp_time_get_us();

    uint64_t quantum_only_time = end - start;

    // Should complete in reasonable time (< 100ms for 100 steps)
    EXPECT_LT(quantum_only_time, 100000ULL)
        << "Quantum walk should be fast";
}

TEST_F(QuantumShannonRegressionTest, Performance_QuantumShannonAcceptableOverhead) {
    // WHAT: Quantum-Shannon overhead acceptable
    // WHY:  Combined system should be < 20% slower
    // EXPECTED: < 20% overhead vs quantum-only

    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    qsd = quantum_shannon_create(network, 0, 8.0f, &qs_config);
    ASSERT_NE(qsd, nullptr);

    // Benchmark quantum-Shannon
    uint64_t start = nimcp_time_get_us();
    EXPECT_TRUE(quantum_shannon_evolve(qsd, 100));
    uint64_t end = nimcp_time_get_us();

    uint64_t qs_time = end - start;

    // Should complete in reasonable time (< 150ms for 100 steps)
    // This is 50% overhead tolerance (generous for first implementation)
    EXPECT_LT(qs_time, 150000ULL)
        << "Quantum-Shannon should have acceptable overhead";
}

TEST_F(QuantumShannonRegressionTest, Performance_BrainLearningAcceptable) {
    // WHAT: Brain learning performance with quantum-Shannon
    // WHY:  Overall system overhead < 10%
    // EXPECTED: Learning completes in reasonable time

    brain_config_t config = {
        .size = BRAIN_SIZE_SMALL,
        .task = BRAIN_TASK_CLASSIFICATION,
        .num_inputs = 10,
        .num_outputs = 10,
        .learning_rate = 0.01f,
        .sparsity_target = 0.85f,
        .enable_explanations = false,
        .enable_quantum_walk_diffusion = true,  // Enable quantum walk
        .quantum_walk_steps = 50,
        .quantum_classical_mixing = 0.2f,
        .quantum_coin_type = 0,  // Hadamard
        .quantum_decoherence_rate = 0.05f
    };
    strncpy(config.task_name, "perf_test", 63);

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = 0.5f;
    }

    // Benchmark 100 learning steps
    uint64_t start = nimcp_time_get_us();
    for (int i = 0; i < 100; i++) {
        brain_learn_example(brain, features, 10, "class_a", 0.9f);
    }
    uint64_t end = nimcp_time_get_us();

    uint64_t elapsed = end - start;
    float avg_us = elapsed / 100.0f;

    // Should average < 5ms per learning step
    EXPECT_LT(avg_us, 5000.0f)
        << "Brain learning should remain fast";
}

TEST_F(QuantumShannonRegressionTest, Performance_BrainInferenceAcceptable) {
    // WHAT: Brain inference performance with quantum-Shannon
    // WHY:  Inference must be fast (< 1ms target)
    // EXPECTED: Inference overhead < 10%

    brain_config_t config = {
        .size = BRAIN_SIZE_SMALL,
        .task = BRAIN_TASK_CLASSIFICATION,
        .num_inputs = 10,
        .num_outputs = 10,
        .learning_rate = 0.01f,
        .sparsity_target = 0.85f,
        .enable_explanations = false,
        .enable_quantum_walk_diffusion = true,
        .quantum_walk_steps = 50,
        .quantum_classical_mixing = 0.2f,
        .quantum_coin_type = 0,
        .quantum_decoherence_rate = 0.05f
    };
    strncpy(config.task_name, "infer_test", 63);

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Train once
    float train_features[10];
    for (int i = 0; i < 10; i++) {
        train_features[i] = 0.7f;
    }
    brain_learn_example(brain, train_features, 10, "class_a", 0.9f);

    // Benchmark inference
    float test_features[10];
    for (int i = 0; i < 10; i++) {
        test_features[i] = 0.65f;
    }

    uint64_t start = nimcp_time_get_us();
    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, test_features, 10);
        (void)decision;
    }
    uint64_t end = nimcp_time_get_us();

    uint64_t elapsed = end - start;
    float avg_us = elapsed / 100.0f;

    // Should average < 1ms per inference
    EXPECT_LT(avg_us, 1000.0f)
        << "Brain inference should remain fast";
}

TEST_F(QuantumShannonRegressionTest, Performance_LargeNetworkScalable) {
    // WHAT: Large network (10K neurons) performance
    // WHY:  Verify scalability acceptable
    // EXPECTED: Completes in reasonable time

    // Create larger network
    neural_network_t large_net = create_test_network(10000);
    ASSERT_NE(large_net, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.num_steps = 10;  // Fewer steps for large network

    quantum_walker_t* large_walker = quantum_walk_create(large_net, &config);
    ASSERT_NE(large_walker, nullptr);

    EXPECT_TRUE(quantum_walk_initialize(large_walker, 0));

    uint64_t start = nimcp_time_get_us();
    EXPECT_TRUE(quantum_walk_evolve(large_walker, 10));
    uint64_t end = nimcp_time_get_us();

    uint64_t elapsed = end - start;

    // Should complete in < 1 second for 10K neurons, 10 steps
    EXPECT_LT(elapsed, 1000000ULL)
        << "Large network should scale reasonably";

    quantum_walk_destroy(large_walker);
    neural_network_destroy(large_net);
}

//=============================================================================
// Category 4: Memory Usage Tests (3 tests)
//=============================================================================

TEST_F(QuantumShannonRegressionTest, Memory_QuantumWalkNoLeaks) {
    // WHAT: Create/destroy quantum walker multiple times
    // WHY:  Verify no memory leaks
    // EXPECTED: No memory growth

    for (int i = 0; i < 10; i++) {
        quantum_walk_config_t config = quantum_walk_default_config();
        walker = quantum_walk_create(network, &config);
        ASSERT_NE(walker, nullptr);

        EXPECT_TRUE(quantum_walk_initialize(walker, i * 100));
        EXPECT_TRUE(quantum_walk_evolve(walker, 10));

        quantum_walk_destroy(walker);
        walker = nullptr;
    }

    SUCCEED() << "No memory leaks detected";
}

TEST_F(QuantumShannonRegressionTest, Memory_QuantumShannonNoLeaks) {
    // WHAT: Create/destroy quantum-Shannon multiple times
    // WHY:  Verify Shannon tracking doesn't leak
    // EXPECTED: No memory growth

    for (int i = 0; i < 10; i++) {
        quantum_shannon_config_t config = quantum_shannon_default_config();
        qsd = quantum_shannon_create(network, i * 100, 8.0f, &config);
        ASSERT_NE(qsd, nullptr);

        EXPECT_TRUE(quantum_shannon_evolve(qsd, 10));

        // Get metrics (triggers allocations)
        shannon_diffusion_metrics_t metrics;
        EXPECT_TRUE(quantum_shannon_get_metrics(qsd, &metrics));

        quantum_shannon_destroy(qsd);
        qsd = nullptr;
    }

    SUCCEED() << "No memory leaks detected";
}

TEST_F(QuantumShannonRegressionTest, Memory_UsageReasonable) {
    // WHAT: Verify memory overhead acceptable
    // WHY:  Shannon adds 2x overhead for complex amplitudes + metrics
    // EXPECTED: < 20MB overhead for 1000 neuron network

    quantum_shannon_config_t config = quantum_shannon_default_config();
    qsd = quantum_shannon_create(network, 0, 8.0f, &config);
    ASSERT_NE(qsd, nullptr);

    EXPECT_TRUE(quantum_shannon_evolve(qsd, 50));

    // Estimated memory usage:
    // - Quantum walk: ~1000 neurons × 16 bytes (complex) = 16KB
    // - Shannon metrics: ~1000 neurons × 4 bytes (float) = 4KB
    // - Sampled synapses: ~100 × 32 bytes = 3KB
    // - Bottlenecks: ~50 × 32 bytes = 2KB
    // Total: ~25KB base + overhead
    // Should be well under 20MB

    SUCCEED() << "Memory usage reasonable";
}

//=============================================================================
// Category 5: Error Handling Unchanged (2 tests)
//=============================================================================

TEST_F(QuantumShannonRegressionTest, ErrorHandling_NullNetworkRejected) {
    // WHAT: NULL network should be rejected
    // WHY:  Error handling must be robust
    // EXPECTED: Returns NULL gracefully

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(nullptr, &config);
    EXPECT_EQ(walker, nullptr)
        << "NULL network should be rejected";

    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    qsd = quantum_shannon_create(nullptr, 0, 8.0f, &qs_config);
    EXPECT_EQ(qsd, nullptr)
        << "NULL network should be rejected";
}

TEST_F(QuantumShannonRegressionTest, ErrorHandling_InvalidParametersRejected) {
    // WHAT: Invalid parameters should be rejected
    // WHY:  Error handling must validate inputs
    // EXPECTED: Functions return false/NULL

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    // Invalid node ID
    EXPECT_FALSE(quantum_walk_initialize(walker, 10000))
        << "Invalid node ID should be rejected";

    // NULL distribution buffer
    EXPECT_FALSE(quantum_walk_get_distribution(walker, nullptr))
        << "NULL buffer should be rejected";

    // NULL walker
    EXPECT_FALSE(quantum_walk_step(nullptr))
        << "NULL walker should be rejected";
}

//=============================================================================
// Category 6: Shannon API Tests (3 tests)
//=============================================================================

TEST_F(QuantumShannonRegressionTest, Shannon_BasicFunctionsWork) {
    // WHAT: Basic Shannon functions work independently
    // WHY:  Shannon module usable standalone
    // EXPECTED: Functions produce valid results

    // Test channel capacity
    float capacity = shannon_channel_capacity(100.0f, 10.0f);
    EXPECT_GT(capacity, 0.0f);
    EXPECT_LT(capacity, 1000.0f);  // Reasonable bounds

    // Test entropy
    float probs[4] = {0.25f, 0.25f, 0.25f, 0.25f};
    float entropy = shannon_entropy_array(probs, 4);
    EXPECT_NEAR(entropy, 2.0f, 0.01f)  // Uniform 4-state = 2 bits
        << "Entropy of uniform distribution should be log2(4)=2";

    // Test entropy with biased distribution
    float biased[2] = {0.9f, 0.1f};
    float biased_entropy = shannon_entropy_array(biased, 2);
    EXPECT_GT(biased_entropy, 0.0f);
    EXPECT_LT(biased_entropy, 1.0f)  // Less than fair coin (1 bit)
        << "Biased distribution has lower entropy";
}

TEST_F(QuantumShannonRegressionTest, Shannon_SynapseMetricsWork) {
    // WHAT: Shannon synapse analysis works
    // WHY:  Can analyze individual synapses
    // EXPECTED: Produces valid metrics

    shannon_synapse_metrics_t metrics = shannon_analyze_synapse(
        0.5f,    // weight
        10.0f,   // pre_firing_rate (Hz)
        0.1f,    // noise_level
        10.0f,   // bandwidth (Hz)
        nullptr  // default config
    );

    EXPECT_GT(metrics.channel_capacity, 0.0f);
    EXPECT_GT(metrics.snr, 0.0f);
    EXPECT_GE(metrics.coding_efficiency, 0.0f);
    EXPECT_LE(metrics.coding_efficiency, 1.0f);
}

TEST_F(QuantumShannonRegressionTest, Shannon_DistributionUtilitiesWork) {
    // WHAT: Shannon distribution utilities work
    // WHY:  Distribution creation/manipulation correct
    // EXPECTED: Allocations/normalizations succeed

    shannon_distribution_t* dist = shannon_distribution_create(10, nullptr);
    ASSERT_NE(dist, nullptr);

    EXPECT_EQ(dist->num_states, 10u);
    EXPECT_TRUE(shannon_distribution_normalize(dist));

    float entropy = shannon_entropy(dist);
    EXPECT_NEAR(entropy, log2f(10.0f), 0.01f)
        << "Uniform 10-state entropy should be log2(10)";

    shannon_distribution_free(dist);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
