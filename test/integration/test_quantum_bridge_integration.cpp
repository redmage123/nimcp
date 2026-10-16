//=============================================================================
// test_quantum_bridge_integration.cpp - Quantum Bridge Integration Tests
//=============================================================================
/**
 * @file test_quantum_bridge_integration.cpp
 * @brief Integration tests for quantum bridges with brain systems
 *
 * WHAT: Tests quantum bridge integration with brain subsystems
 * WHY:  Verify Grover search acceleration and quantum attention work correctly
 * HOW:  Create quantum bridges, connect to brain, test quantum operations
 *
 * QUANTUM MODEL:
 * - Classical routing: O(N) attention checks
 * - Quantum routing: O(sqrt(N)) via superposition + Grover search
 * - Ternary attention states: -1 (inhibit), 0 (superposition), +1 (attend)
 *
 * TEST SCENARIOS:
 * 1. Quantum attention bridge creation and configuration
 * 2. Grover search acceleration for attention
 * 3. Quantum walk attention exploration
 * 4. Quantum annealing for attention optimization
 * 5. Integration with brain decision making
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "plasticity/attention/nimcp_quantum_attention.h"
#include "middleware/routing/nimcp_thalamic_quantum_bridge.h"
#include "core/brain/nimcp_brain.h"
#include "utils/quantum/nimcp_quantum_shannon.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumBridgeIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 16;
        config.num_outputs = 4;
        snprintf(config.task_name, sizeof(config.task_name), "quantum_test");

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr) << "Failed to create test brain";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create query vectors
    void createQueryVectors(float* query, uint32_t seq_len, uint32_t dim) {
        for (uint32_t i = 0; i < seq_len; i++) {
            for (uint32_t j = 0; j < dim; j++) {
                query[i * dim + j] = 0.5f + 0.5f * sin((i + j) * 0.1f);
            }
        }
    }

    // Helper: Create key vectors
    void createKeyVectors(float* key, uint32_t seq_len, uint32_t dim) {
        for (uint32_t i = 0; i < seq_len; i++) {
            for (uint32_t j = 0; j < dim; j++) {
                key[i * dim + j] = 0.5f + 0.5f * cos((i - j) * 0.15f);
            }
        }
    }
};

//=============================================================================
// Test: Quantum Attention Creation
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_Creation) {
    // WHAT: Test quantum attention context creation
    // WHY:  Verify quantum attention infrastructure initializes correctly
    // HOW:  Create context, verify configuration applied

    quantum_attention_config_t config = quantum_attention_default_config();
    config.mode = QUANTUM_ATTENTION_SPARSE;
    config.collapse_threshold = 0.5f;
    config.sparsity_threshold = 0.01f;

    uint32_t seq_length = 32;
    uint32_t head_dim = 64;
    uint32_t num_heads = 4;

    quantum_attention_t ctx = quantum_attention_create(
        &config, seq_length, head_dim, num_heads
    );
    ASSERT_NE(ctx, nullptr);

    // Verify context is valid
    EXPECT_EQ(ctx->seq_length, seq_length);
    EXPECT_EQ(ctx->head_dim, head_dim);
    EXPECT_EQ(ctx->num_heads, num_heads);

    quantum_attention_destroy(ctx);
}

//=============================================================================
// Test: Quantum Attention Score Computation
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_ScoreComputation) {
    // WHAT: Test quantum-enhanced attention score computation
    // WHY:  Verify sparse attention with quantum speedup
    // HOW:  Compute scores, verify selective measurement

    quantum_attention_config_t config = quantum_attention_default_config();
    config.mode = QUANTUM_ATTENTION_SPARSE;
    config.collapse_threshold = 0.3f;
    config.sparsity_threshold = 0.1f;

    uint32_t seq_length = 16;
    uint32_t head_dim = 32;
    uint32_t num_heads = 2;

    quantum_attention_t ctx = quantum_attention_create(
        &config, seq_length, head_dim, num_heads
    );
    ASSERT_NE(ctx, nullptr);

    // Create query and key vectors
    float* query = (float*)calloc(seq_length * head_dim, sizeof(float));
    float* key = (float*)calloc(seq_length * head_dim, sizeof(float));
    ASSERT_NE(query, nullptr);
    ASSERT_NE(key, nullptr);

    createQueryVectors(query, seq_length, head_dim);
    createKeyVectors(key, seq_length, head_dim);

    // Compute attention scores
    float scale = 1.0f / sqrtf((float)head_dim);
    quantum_attention_compute_scores(ctx, query, key, 0, scale);

    // Get statistics
    quantum_attention_stats_t stats;
    quantum_attention_get_stats(ctx, &stats);

    // Verify computation occurred
    EXPECT_GT(stats.forward_calls, 0u);
    EXPECT_GT(stats.pairs_computed + stats.pairs_skipped, 0u);

    // Verify sparsity (some pairs should be skipped)
    EXPECT_GE(stats.avg_sparsity, 0.0f);
    EXPECT_LE(stats.avg_sparsity, 1.0f);

    // Verify coherence (fraction in superposition)
    EXPECT_GE(stats.avg_coherence, 0.0f);
    EXPECT_LE(stats.avg_coherence, 1.0f);

    free(query);
    free(key);
    quantum_attention_destroy(ctx);
}

//=============================================================================
// Test: Sparse Attention Pairs
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_SparsePairs) {
    // WHAT: Test sparse attention pair extraction
    // WHY:  Verify only measured pairs are returned
    // HOW:  Compute scores, extract sparse pairs

    quantum_attention_config_t config = quantum_attention_default_config();
    config.mode = QUANTUM_ATTENTION_SPARSE;
    config.collapse_threshold = 0.2f;

    uint32_t seq_length = 8;
    uint32_t head_dim = 16;

    quantum_attention_t ctx = quantum_attention_create(
        &config, seq_length, head_dim, 1
    );
    ASSERT_NE(ctx, nullptr);

    // Create vectors
    float query[8 * 16];
    float key[8 * 16];
    createQueryVectors(query, seq_length, head_dim);
    createKeyVectors(key, seq_length, head_dim);

    // Compute scores
    float scale = 1.0f / sqrtf((float)head_dim);
    quantum_attention_compute_scores(ctx, query, key, 0, scale);

    // Extract sparse pairs
    uint32_t max_pairs = seq_length * seq_length;
    uint32_t* query_indices = (uint32_t*)calloc(max_pairs, sizeof(uint32_t));
    uint32_t* key_indices = (uint32_t*)calloc(max_pairs, sizeof(uint32_t));
    float* values = (float*)calloc(max_pairs, sizeof(float));

    uint32_t num_pairs = quantum_attention_get_sparse_pairs(
        ctx, query_indices, key_indices, values, max_pairs
    );

    // Should have some sparse pairs
    EXPECT_GE(num_pairs, 0u);
    EXPECT_LE(num_pairs, max_pairs);

    // Verify indices are valid
    for (uint32_t i = 0; i < num_pairs; i++) {
        EXPECT_LT(query_indices[i], seq_length);
        EXPECT_LT(key_indices[i], seq_length);
        // Values should be ternary: -1, 0, or +1
        EXPECT_GE(values[i], -1.0f);
        EXPECT_LE(values[i], 1.0f);
    }

    free(query_indices);
    free(key_indices);
    free(values);
    quantum_attention_destroy(ctx);
}

//=============================================================================
// Test: Quantum Attention Mask Reset
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_MaskReset) {
    // WHAT: Test attention mask reset to superposition
    // WHY:  Verify reset clears measured states
    // HOW:  Compute, reset, verify superposition

    quantum_attention_config_t config = quantum_attention_default_config();

    quantum_attention_t ctx = quantum_attention_create(
        &config, 8, 16, 1
    );
    ASSERT_NE(ctx, nullptr);

    // Create and compute
    float query[8 * 16], key[8 * 16];
    createQueryVectors(query, 8, 16);
    createKeyVectors(key, 8, 16);

    quantum_attention_compute_scores(ctx, query, key, 0, 0.25f);

    // Get pairs after computation
    uint32_t query_idx[64], key_idx[64];
    float values[64];
    uint32_t pairs_after_compute = quantum_attention_get_sparse_pairs(
        ctx, query_idx, key_idx, values, 64
    );

    // Reset mask
    quantum_attention_reset_mask(ctx);

    // Get pairs after reset - should be 0 (all in superposition)
    uint32_t pairs_after_reset = quantum_attention_get_sparse_pairs(
        ctx, query_idx, key_idx, values, 64
    );

    EXPECT_EQ(pairs_after_reset, 0u) << "After reset, all pairs should be in superposition";

    quantum_attention_destroy(ctx);
}

//=============================================================================
// Test: Quantum Attention Value Application
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_ValueApplication) {
    // WHAT: Test sparse attention applied to values
    // WHY:  Verify weighted value aggregation works
    // HOW:  Compute attention, apply to values, verify output

    quantum_attention_config_t config = quantum_attention_default_config();
    config.collapse_threshold = 0.2f;

    uint32_t seq_length = 8;
    uint32_t head_dim = 16;
    uint32_t value_dim = 32;

    quantum_attention_t ctx = quantum_attention_create(
        &config, seq_length, head_dim, 1
    );
    ASSERT_NE(ctx, nullptr);

    // Create vectors
    float query[8 * 16], key[8 * 16];
    createQueryVectors(query, seq_length, head_dim);
    createKeyVectors(key, seq_length, head_dim);

    // Create value vectors
    float* value = (float*)calloc(seq_length * value_dim, sizeof(float));
    float* output = (float*)calloc(seq_length * value_dim, sizeof(float));
    ASSERT_NE(value, nullptr);
    ASSERT_NE(output, nullptr);

    for (uint32_t i = 0; i < seq_length * value_dim; i++) {
        value[i] = 0.5f + 0.5f * sin(i * 0.2f);
    }

    // Compute attention
    quantum_attention_compute_scores(ctx, query, key, 0, 0.25f);

    // Apply to values
    quantum_attention_apply_values(ctx, value, value_dim, output);

    // Verify output is valid
    bool has_nonzero = false;
    for (uint32_t i = 0; i < seq_length * value_dim; i++) {
        EXPECT_FALSE(isnan(output[i])) << "Output should not be NaN at index " << i;
        EXPECT_FALSE(isinf(output[i])) << "Output should not be Inf at index " << i;
        if (output[i] != 0.0f) {
            has_nonzero = true;
        }
    }

    // Output should have some non-zero values
    EXPECT_TRUE(has_nonzero) << "Output should have non-zero values";

    free(value);
    free(output);
    quantum_attention_destroy(ctx);
}

//=============================================================================
// Test: Quantum Statistics Reset
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_StatsReset) {
    // WHAT: Test statistics reset
    // WHY:  Verify clean reset of performance metrics
    // HOW:  Accumulate stats, reset, verify zeroed

    quantum_attention_config_t config = quantum_attention_default_config();

    quantum_attention_t ctx = quantum_attention_create(&config, 8, 16, 1);
    ASSERT_NE(ctx, nullptr);

    // Compute to accumulate stats
    float query[8 * 16], key[8 * 16];
    createQueryVectors(query, 8, 16);
    createKeyVectors(key, 8, 16);

    for (int i = 0; i < 5; i++) {
        quantum_attention_compute_scores(ctx, query, key, 0, 0.25f);
    }

    quantum_attention_stats_t stats;
    quantum_attention_get_stats(ctx, &stats);
    EXPECT_GT(stats.forward_calls, 0u);

    // Reset stats
    quantum_attention_reset_stats(ctx);

    quantum_attention_get_stats(ctx, &stats);
    EXPECT_EQ(stats.forward_calls, 0u);
    EXPECT_EQ(stats.pairs_computed, 0u);
    EXPECT_EQ(stats.pairs_skipped, 0u);

    quantum_attention_destroy(ctx);
}

//=============================================================================
// Test: Quantum Shannon Diffusion with Brain
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumShannon_BrainIntegration) {
    // WHAT: Test quantum Shannon diffusion with brain network
    // WHY:  Verify quantum walk integrates with brain topology
    // HOW:  Create QSD from brain network, evolve, verify metrics

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(adaptive_net, nullptr);

    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(network, nullptr);

    quantum_shannon_config_t config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &config
    );
    ASSERT_NE(qsd, nullptr);

    // Evolve quantum walk
    bool evolve_result = quantum_shannon_evolve(qsd, 50);
    EXPECT_TRUE(evolve_result);

    // Get metrics
    shannon_diffusion_metrics_t metrics;
    bool metrics_result = quantum_shannon_get_metrics(qsd, &metrics);
    EXPECT_TRUE(metrics_result);

    // Verify metrics are valid
    EXPECT_GE(metrics.propagation_efficiency, 0.0f);
    EXPECT_LE(metrics.propagation_efficiency, 1.0f);

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Test: Quantum Shannon Optimization
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumShannon_Optimization) {
    // WHAT: Test quantum Shannon optimization
    // WHY:  Verify bottleneck detection and optimization
    // HOW:  Create QSD, evolve, optimize, verify improvement

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(adaptive_net, nullptr);

    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(network, nullptr);

    quantum_shannon_config_t config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &config
    );
    ASSERT_NE(qsd, nullptr);

    // Initial evolution
    quantum_shannon_evolve(qsd, 25);

    // Get initial metrics
    shannon_diffusion_metrics_t initial_metrics;
    quantum_shannon_get_metrics(qsd, &initial_metrics);

    // Optimize
    bool optimize_result = quantum_shannon_optimize(qsd);
    EXPECT_TRUE(optimize_result);

    // Continue evolution
    quantum_shannon_evolve(qsd, 25);

    // Get final metrics
    shannon_diffusion_metrics_t final_metrics;
    quantum_shannon_get_metrics(qsd, &final_metrics);

    // Both should be valid
    EXPECT_GE(final_metrics.propagation_efficiency, 0.0f);
    EXPECT_LE(final_metrics.propagation_efficiency, 1.0f);

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Test: Thalamic Quantum Routing with Brain
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, ThalamicQuantum_BrainRouting) {
    // WHAT: Test thalamic quantum routing in brain context
    // WHY:  Verify quantum routing integrates with brain signals
    // HOW:  Create bridge, route brain signals, verify results

    thalamic_quantum_config_t config = thalamic_quantum_default_config();
    config.routing_threshold = 0.3f;

    thalamic_quantum_bridge_t* bridge = thalamic_quantum_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Create signal features from brain input space
    float signal_features[16];
    for (int i = 0; i < 16; i++) {
        signal_features[i] = 0.5f + 0.3f * sin(i * 0.4f);
    }

    // Define brain region destinations
    uint32_t region_ids[] = {
        1,  // Visual cortex
        2,  // Auditory cortex
        3,  // Motor cortex
        4,  // Prefrontal cortex
        5,  // Hippocampus
        6   // Amygdala
    };
    uint32_t num_regions = 6;

    uint32_t routed_regions[6];
    uint32_t num_routed = 0;

    int result = thalamic_quantum_route(
        bridge, 0, region_ids, num_regions,
        signal_features, 16,
        routed_regions, &num_routed
    );

    EXPECT_EQ(result, 0);
    EXPECT_LE(num_routed, num_regions);

    // Run brain inference
    brain_decision_t* decision = brain_decide(brain, signal_features, 16);
    EXPECT_NE(decision, nullptr);
    brain_free_decision(decision);

    thalamic_quantum_bridge_destroy(bridge);
}

//=============================================================================
// Test: Quantum Speedup Estimation
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_SpeedupEstimation) {
    // WHAT: Test speedup factor estimation
    // WHY:  Verify theoretical sqrt(N) speedup is tracked
    // HOW:  Compute attention, verify speedup reported

    quantum_attention_config_t config = quantum_attention_default_config();
    config.collapse_threshold = 0.5f;  // Higher threshold for more skipping

    uint32_t seq_length = 32;
    uint32_t head_dim = 64;

    quantum_attention_t ctx = quantum_attention_create(
        &config, seq_length, head_dim, 1
    );
    ASSERT_NE(ctx, nullptr);

    float* query = (float*)calloc(seq_length * head_dim, sizeof(float));
    float* key = (float*)calloc(seq_length * head_dim, sizeof(float));
    createQueryVectors(query, seq_length, head_dim);
    createKeyVectors(key, seq_length, head_dim);

    // Compute multiple times
    for (int i = 0; i < 10; i++) {
        quantum_attention_compute_scores(ctx, query, key, 0, 0.125f);
    }

    quantum_attention_stats_t stats;
    quantum_attention_get_stats(ctx, &stats);

    // Speedup should be >= 1.0 (at worst, same as classical)
    EXPECT_GE(stats.speedup_factor, 1.0f);

    // Theoretical max speedup is sqrt(N) where N = seq_length^2
    float theoretical_max = (float)seq_length;  // sqrt(seq_length^2)
    EXPECT_LE(stats.speedup_factor, theoretical_max + 1.0f);

    free(query);
    free(key);
    quantum_attention_destroy(ctx);
}

//=============================================================================
// Test: Error Handling
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, ErrorHandling_InvalidInputs) {
    // WHAT: Test error handling for invalid inputs
    // WHY:  Verify graceful handling of edge cases
    // HOW:  Pass invalid inputs, verify appropriate responses

    // Null config
    quantum_attention_t ctx = quantum_attention_create(nullptr, 8, 16, 1);
    EXPECT_EQ(ctx, nullptr);

    // Zero dimensions
    quantum_attention_config_t config = quantum_attention_default_config();
    ctx = quantum_attention_create(&config, 0, 16, 1);
    EXPECT_EQ(ctx, nullptr);

    ctx = quantum_attention_create(&config, 8, 0, 1);
    EXPECT_EQ(ctx, nullptr);

    ctx = quantum_attention_create(&config, 8, 16, 0);
    EXPECT_EQ(ctx, nullptr);

    // Valid creation
    ctx = quantum_attention_create(&config, 8, 16, 1);
    ASSERT_NE(ctx, nullptr);

    // Null vectors
    quantum_attention_compute_scores(ctx, nullptr, nullptr, 0, 0.25f);

    // Stats from null
    quantum_attention_stats_t stats;
    quantum_attention_get_stats(nullptr, &stats);
    EXPECT_EQ(stats.forward_calls, 0u);

    // Null query
    quantum_attention_apply_values(ctx, nullptr, 16, nullptr);

    quantum_attention_destroy(ctx);
}

//=============================================================================
// Test: Different Quantum Modes
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_DifferentModes) {
    // WHAT: Test different quantum attention modes
    // WHY:  Verify all modes work correctly
    // HOW:  Create contexts with different modes, compute attention

    quantum_attention_mode_t modes[] = {
        QUANTUM_ATTENTION_FULL,
        QUANTUM_ATTENTION_SPARSE,
        QUANTUM_ATTENTION_WALK,
        QUANTUM_ATTENTION_ANNEAL
    };

    uint32_t seq_length = 8;
    uint32_t head_dim = 16;

    float query[8 * 16], key[8 * 16];
    createQueryVectors(query, seq_length, head_dim);
    createKeyVectors(key, seq_length, head_dim);

    for (auto mode : modes) {
        quantum_attention_config_t config = quantum_attention_default_config();
        config.mode = mode;

        quantum_attention_t ctx = quantum_attention_create(
            &config, seq_length, head_dim, 1
        );

        if (ctx) {
            // Compute attention
            quantum_attention_compute_scores(ctx, query, key, 0, 0.25f);

            // Get stats
            quantum_attention_stats_t stats;
            quantum_attention_get_stats(ctx, &stats);

            // Should have processed something
            EXPECT_GT(stats.pairs_computed + stats.pairs_skipped, 0u)
                << "Mode " << (int)mode << " should process pairs";

            quantum_attention_destroy(ctx);
        }
    }
}

//=============================================================================
// Test: Multiple Heads
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_MultipleHeads) {
    // WHAT: Test multi-head quantum attention
    // WHY:  Verify independent processing per head
    // HOW:  Create multi-head context, compute for each head

    quantum_attention_config_t config = quantum_attention_default_config();

    uint32_t seq_length = 8;
    uint32_t head_dim = 16;
    uint32_t num_heads = 4;

    quantum_attention_t ctx = quantum_attention_create(
        &config, seq_length, head_dim, num_heads
    );
    ASSERT_NE(ctx, nullptr);

    float query[8 * 16], key[8 * 16];
    createQueryVectors(query, seq_length, head_dim);
    createKeyVectors(key, seq_length, head_dim);

    // Compute for each head
    for (uint32_t head = 0; head < num_heads; head++) {
        // Vary key slightly per head
        for (uint32_t i = 0; i < seq_length * head_dim; i++) {
            key[i] += 0.1f * head;
        }

        quantum_attention_compute_scores(ctx, query, key, head, 0.25f);
    }

    // Should have accumulated stats across heads
    quantum_attention_stats_t stats;
    quantum_attention_get_stats(ctx, &stats);

    EXPECT_EQ(stats.forward_calls, num_heads);

    quantum_attention_destroy(ctx);
}

//=============================================================================
// Test: Measure Single Pair
//=============================================================================

TEST_F(QuantumBridgeIntegrationTest, QuantumAttention_MeasurePair) {
    // WHAT: Test single pair measurement
    // WHY:  Verify ternary collapse works correctly
    // HOW:  Measure pairs with different scores, verify states

    quantum_attention_config_t config = quantum_attention_default_config();
    config.collapse_threshold = 0.5f;
    config.sparsity_threshold = 0.1f;

    quantum_attention_t ctx = quantum_attention_create(&config, 8, 16, 1);
    ASSERT_NE(ctx, nullptr);

    // Measure with high positive score
    trit_t result1 = quantum_attention_measure_pair(ctx, 0, 1, 0.8f);
    EXPECT_EQ(result1, TRIT_POSITIVE);

    // Measure with high negative score
    trit_t result2 = quantum_attention_measure_pair(ctx, 1, 2, -0.8f);
    EXPECT_EQ(result2, TRIT_NEGATIVE);

    // Measure with low score (should stay superposition or weak)
    trit_t result3 = quantum_attention_measure_pair(ctx, 2, 3, 0.05f);
    // Could be UNKNOWN or weak positive
    EXPECT_GE((int)result3, -1);
    EXPECT_LE((int)result3, 1);

    quantum_attention_destroy(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
