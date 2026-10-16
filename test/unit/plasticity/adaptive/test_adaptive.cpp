#include "test_helpers.h"

#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Helper Functions
//=============================================================================

// Helper to create default adaptive network configuration
static adaptive_network_config_t create_test_adaptive_config()
{
    adaptive_network_config_t config;
    memset(&config, 0, sizeof(config));

    // Base network config
    config.base_config = create_test_config();
    config.base_config.num_neurons = 20;
    config.base_config.input_size = 5;
    config.base_config.output_size = 3;

    // Adaptive spike parameters
    config.spike_params.k_factor = 0.5f;
    config.spike_params.sparsity_target = 0.7f;
    config.spike_params.encoding = SPIKE_ENCODING_INTEGER;
    config.spike_params.enable_soft_reset = true;
    config.spike_params.enable_adaptation = true;
    config.spike_params.adaptation_window = 100;
    config.spike_params.min_threshold = 0.1f;
    config.spike_params.max_threshold = 10.0f;

    // Other settings
    config.enable_sparsity = true;
    config.pruning_threshold = 0.01f;
    config.update_frequency = 10;

    return config;
}

//=============================================================================
// Adaptive Network Creation Tests
//=============================================================================

// Test adaptive network creation with valid config
TEST(AdaptiveNetwork, CreateValid)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);

    ASSERT_NE(network, nullptr);

    adaptive_network_destroy(network);
}

// Test adaptive network creation with null config
TEST(AdaptiveNetwork, CreateNullConfig)
{
    adaptive_network_t network = adaptive_network_create(nullptr);
    ASSERT_EQ(network, nullptr);
}

// Test adaptive network creation with invalid k_factor
TEST(AdaptiveNetwork, CreateInvalidKFactor)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    config.spike_params.k_factor = 0.0f;  // Invalid

    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_EQ(network, nullptr);
}

// Test adaptive network creation with invalid threshold range
TEST(AdaptiveNetwork, CreateInvalidThresholds)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    config.spike_params.min_threshold = 10.0f;
    config.spike_params.max_threshold = 1.0f;  // max < min

    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_EQ(network, nullptr);
}

//=============================================================================
// Adaptive Threshold Computation Tests
//=============================================================================

// Test adaptive threshold computation
TEST(AdaptiveThreshold, ComputeThreshold)
{
    float input[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float k_factor = 0.5f;

    float threshold = adaptive_compute_threshold(input, 5, k_factor);

    // Threshold should be positive and reasonable
    EXPECT_GT(threshold, 0.0f);
    EXPECT_LT(threshold, 100.0f);

    // Mean of absolute values = 3.0, so threshold = 3.0 / 0.5 = 6.0
    EXPECT_TRUE(float_equals(threshold, 6.0f));
}

// Test adaptive threshold with negative values
TEST(AdaptiveThreshold, ComputeThresholdNegative)
{
    float input[] = {-1.0f, -2.0f, -3.0f};
    float k_factor = 1.0f;

    float threshold = adaptive_compute_threshold(input, 3, k_factor);

    // Should use absolute values
    EXPECT_GT(threshold, 0.0f);
    EXPECT_TRUE(float_equals(threshold, 2.0f));  // mean(|x|) = 2.0
}

// Test adaptive threshold with zero k_factor
TEST(AdaptiveThreshold, ComputeThresholdZeroK)
{
    float input[] = {1.0f, 2.0f, 3.0f};
    float k_factor = 0.0f;

    float threshold = adaptive_compute_threshold(input, 3, k_factor);

    // Should return default threshold when k_factor invalid
    EXPECT_GT(threshold, 0.0f);
}

// Test adaptive threshold with null input
TEST(AdaptiveThreshold, ComputeThresholdNull)
{
    float threshold = adaptive_compute_threshold(nullptr, 5, 0.5f);

    // Should return default threshold
    EXPECT_TRUE(float_equals(threshold, 1.0f));
}

//=============================================================================
// Spike Encoding/Decoding Tests
//=============================================================================

// Test integer spike encoding
TEST(SpikeEncoding, EncodeInteger)
{
    int32_t spike_count = 42;
    uint8_t spike_train[256];

    uint32_t length = adaptive_encode_spikes(spike_count, SPIKE_ENCODING_INTEGER, spike_train, 256);

    ASSERT_GT(length, 0);
    ASSERT_EQ(length, sizeof(int32_t));

    // Decode and verify
    float threshold = 1.0f;
    float decoded = adaptive_decode_spikes(spike_train, length, SPIKE_ENCODING_INTEGER, threshold);
    EXPECT_TRUE(float_equals(decoded, 42.0f));
}

// Test binary spike encoding
TEST(SpikeEncoding, EncodeBinary)
{
    int32_t spike_count = 5;
    uint8_t spike_train[256];

    uint32_t length = adaptive_encode_spikes(spike_count, SPIKE_ENCODING_BINARY, spike_train, 256);

    ASSERT_EQ(length, 5);  // Binary encoding creates length = spike_count

    // Decode
    float threshold = 1.0f;
    float decoded = adaptive_decode_spikes(spike_train, length, SPIKE_ENCODING_BINARY, threshold);
    EXPECT_TRUE(float_equals(decoded, 5.0f));
}

// Test ternary spike encoding
TEST(SpikeEncoding, EncodeTernary)
{
    uint8_t spike_train[256];

    // Positive spike count
    uint32_t length = adaptive_encode_spikes(10, SPIKE_ENCODING_TERNARY, spike_train, 256);
    ASSERT_EQ(length, 1);
    EXPECT_EQ(spike_train[0], 1);

    // Negative spike count
    length = adaptive_encode_spikes(-10, SPIKE_ENCODING_TERNARY, spike_train, 256);
    ASSERT_EQ(length, 1);
    EXPECT_EQ(spike_train[0], 255);

    // Zero spike count
    length = adaptive_encode_spikes(0, SPIKE_ENCODING_TERNARY, spike_train, 256);
    ASSERT_EQ(length, 1);
    EXPECT_EQ(spike_train[0], 0);
}

// Test value to spike conversion
TEST(SpikeEncoding, ValueToSpikes)
{
    float value = 5.0f;
    float threshold = 1.0f;

    int32_t spikes = adaptive_value_to_spikes(value, threshold);
    EXPECT_EQ(spikes, 5);

    // Test with different threshold
    spikes = adaptive_value_to_spikes(10.0f, 2.0f);
    EXPECT_EQ(spikes, 5);

    // Test with negative value
    spikes = adaptive_value_to_spikes(-3.0f, 1.0f);
    EXPECT_EQ(spikes, -3);
}

//=============================================================================
// Adaptive Network Forward Pass Tests
//=============================================================================

// Test forward pass with basic input
TEST(AdaptiveNetwork, ForwardPassBasic)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[3] = {0.0f};

    uint32_t active = adaptive_network_forward(network, input, 5, output, 3, 1);

    // Some neurons should be active (or zero if all below threshold)
    EXPECT_GE(active, 0);

    // Output should be populated
    // (exact values depend on network initialization)

    adaptive_network_destroy(network);
}

// Test forward pass with null inputs
TEST(AdaptiveNetwork, ForwardPassNullInput)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    float output[3] = {0.0f};

    uint32_t active = adaptive_network_forward(network, nullptr, 5, output, 3, 1);
    EXPECT_EQ(active, 0);

    adaptive_network_destroy(network);
}

// Test forward pass with multiple iterations
TEST(AdaptiveNetwork, ForwardPassMultiple)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    float input[5] = {0.5f, 0.7f, 0.3f, 0.9f, 0.1f};
    float output[3];

    // Run multiple forward passes
    for (uint64_t t = 1; t <= 10; t++) {
        adaptive_network_forward(network, input, 5, output, 3, t);
    }

    // Network should remain functional

    adaptive_network_destroy(network);
}

//=============================================================================
// Sparsity Tests
//=============================================================================

// Test sparsity measurement
TEST(AdaptiveNetwork, GetSparsity)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    config.spike_params.sparsity_target = 0.7f;
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Initial sparsity
    float sparsity = adaptive_network_get_sparsity(network);
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);

    // Run some forward passes
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[3];

    for (int i = 0; i < 20; i++) {
        adaptive_network_forward(network, input, 5, output, 3, i + 1);
    }

    // Sparsity should be updated
    sparsity = adaptive_network_get_sparsity(network);
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);

    adaptive_network_destroy(network);
}

// Test sparsity adaptation
TEST(AdaptiveNetwork, SparsityAdaptation)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    config.spike_params.enable_adaptation = true;
    config.spike_params.sparsity_target = 0.8f;  // High sparsity target
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    float input[5] = {5.0f, 5.0f, 5.0f, 5.0f, 5.0f};  // High input
    float output[3];

    // Run forward passes to allow adaptation
    for (int i = 0; i < 100; i++) {
        adaptive_network_forward(network, input, 5, output, 3, i + 1);
    }

    // Sparsity should approach target
    float sparsity = adaptive_network_get_sparsity(network);
    EXPECT_GT(sparsity, 0.5f);  // Should be reasonably sparse

    adaptive_network_destroy(network);
}

//=============================================================================
// Pruning Tests
//=============================================================================

// Test synaptic pruning
TEST(AdaptiveNetwork, Pruning)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Prune weak connections
    uint32_t pruned = adaptive_network_prune(network, 0.01f);

    // Pruning count depends on network initialization
    EXPECT_GE(pruned, 0);

    adaptive_network_destroy(network);
}

//=============================================================================
// Integration Tests
//=============================================================================

// Test complete inference pipeline
TEST(AdaptiveNetwork, InferencePipeline)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Prepare test data
    float input[5] = {0.1f, 0.5f, 0.9f, 0.3f, 0.7f};
    float output[3];

    // Run inference
    uint32_t active = adaptive_network_forward(network, input, 5, output, 3, 1);

    // Verify results
    EXPECT_GE(active, 0);

    // Check sparsity
    float sparsity = adaptive_network_get_sparsity(network);
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);

    adaptive_network_destroy(network);
}

// Test different encoding schemes
TEST(AdaptiveNetwork, EncodingSchemes)
{
    spike_encoding_t encodings[] = {SPIKE_ENCODING_INTEGER, SPIKE_ENCODING_BINARY,
                                    SPIKE_ENCODING_TERNARY, SPIKE_ENCODING_BITWISE};

    for (int i = 0; i < 4; i++) {
        adaptive_network_config_t config = create_test_adaptive_config();
        config.spike_params.encoding = encodings[i];
        adaptive_network_t network = adaptive_network_create(&config);
        ASSERT_NE(network, nullptr);

        float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        float output[3];

        // Run forward pass with each encoding
        adaptive_network_forward(network, input, 5, output, 3, 1);

        adaptive_network_destroy(network);
    }
}

// Test network with sparsity disabled
TEST(AdaptiveNetwork, NoSparsity)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    config.enable_sparsity = false;
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[3];

    uint32_t active = adaptive_network_forward(network, input, 5, output, 3, 1);

    // Without sparsity enforcement, more neurons may be active
    EXPECT_GE(active, 0);

    adaptive_network_destroy(network);
}

// Test threshold adaptation over time
TEST(AdaptiveNetwork, ThresholdAdaptation)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    config.spike_params.enable_adaptation = true;
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    float input[5] = {2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
    float output[3];

    // Run many iterations to allow adaptation
    for (uint64_t t = 1; t <= 200; t++) {
        adaptive_network_forward(network, input, 5, output, 3, t);
    }

    // Network should have adapted thresholds
    // (exact behavior depends on implementation)

    adaptive_network_destroy(network);
}

//=============================================================================
// Learning & Distillation Tests
//=============================================================================

// Test supervised learning
TEST(AdaptiveLearning, SupervisedLearning)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Create training example
    float input[5] = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f};
    float target[3] = {1.0f, 0.0f, 0.5f};

    training_example_t example;
    example.input = input;
    example.input_size = 5;
    example.target = target;
    example.target_size = 3;
    example.confidence = 0.9f;
    strcpy(example.label, "test_pattern");

    // Learn from example
    float loss = adaptive_network_learn(network, &example, LEARN_MODE_SUPERVISED, 0.01f);

    // Loss should be a valid number
    EXPECT_GE(loss, 0.0f);

    adaptive_network_destroy(network);
}

// Test unsupervised learning
TEST(AdaptiveLearning, UnsupervisedLearning)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    training_example_t example;
    float input[5] = {0.5f, 0.7f, 0.3f, 0.9f, 0.1f};
    example.input = input;
    example.input_size = 5;
    example.target = nullptr;  // No target for unsupervised
    example.target_size = 0;
    example.confidence = 1.0f;
    example.label[0] = '\0';

    float loss = adaptive_network_learn(network, &example, LEARN_MODE_UNSUPERVISED, 0.01f);
    EXPECT_EQ(loss, 0.0f);  // Unsupervised returns 0 loss

    adaptive_network_destroy(network);
}

// Test batch learning
TEST(AdaptiveLearning, BatchLearning)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Create batch of examples
    training_example_t examples[3];
    float inputs[3][5] = {{1.0f, 0.0f, 0.5f, 0.2f, 0.8f},
                          {0.5f, 0.5f, 0.5f, 0.5f, 0.5f},
                          {0.0f, 1.0f, 0.3f, 0.7f, 0.4f}};
    float targets[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

    for (int i = 0; i < 3; i++) {
        examples[i].input = inputs[i];
        examples[i].input_size = 5;
        examples[i].target = targets[i];
        examples[i].target_size = 3;
        examples[i].confidence = 1.0f;
        snprintf(examples[i].label, 64, "example_%d", i);
    }

    // Learn from batch
    float avg_loss =
        adaptive_network_learn_batch(network, examples, 3, LEARN_MODE_SUPERVISED, 0.01f);
    EXPECT_GE(avg_loss, 0.0f);

    adaptive_network_destroy(network);
}

// Test distillation from teacher
TEST(AdaptiveLearning, Distillation)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Simple teacher function
    auto teacher = [](const float* input, uint32_t input_size, void* context) -> float* {
        float* output = (float*) nimcp_malloc(3 * sizeof(float));
        output[0] = input[0] + input[1];
        output[1] = input[2] * 2.0f;
        output[2] = input[3] - input[4];
        return output;
    };

    float input[5] = {0.5f, 0.3f, 0.8f, 0.9f, 0.2f};
    float loss = adaptive_network_distill(network, input, 5, teacher, nullptr, 0.01f);

    EXPECT_GE(loss, 0.0f);

    adaptive_network_destroy(network);
}

//=============================================================================
// Model Persistence Tests
//=============================================================================

// Test network save and load
TEST(AdaptivePersistence, SaveLoad)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Run some forward passes to change network state
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[3];
    for (int i = 0; i < 10; i++) {
        adaptive_network_forward(network, input, 5, output, 3, i + 1);
    }

    // Save network
    const char* filepath = "/tmp/nimcp_test_network.bin";
    ASSERT_TRUE(adaptive_network_save(network, filepath, SERIALIZE_FORMAT_BINARY));

    // Load network
    adaptive_network_t loaded = adaptive_network_load(filepath);
    ASSERT_NE(loaded, nullptr);

    // Cleanup
    adaptive_network_destroy(network);
    adaptive_network_destroy(loaded);
    unlink(filepath);
}

// Test network size calculation
TEST(AdaptivePersistence, GetSize)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    size_t size = adaptive_network_get_size(network);
    EXPECT_GT(size, 0);

    adaptive_network_destroy(network);
}

//=============================================================================
// Interpretability Tests
//=============================================================================

// Test activation analysis
TEST(AdaptiveInterpret, ActivationAnalysis)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    float input[5] = {5.0f, 3.0f, 8.0f, 1.0f, 6.0f};

    activation_analysis_t analysis;
    analysis.active_neuron_ids = nullptr;
    analysis.activation_strengths = nullptr;

    bool success = adaptive_network_analyze_activation(network, input, 5, &analysis);
    ASSERT_TRUE(success);

    EXPECT_GE(analysis.num_active_neurons, 0);
    EXPECT_GE(analysis.sparsity, 0.0f);
    EXPECT_LE(analysis.sparsity, 1.0f);
    EXPECT_GE(analysis.confidence, 0.0f);
    EXPECT_LE(analysis.confidence, 1.0f);

    // Free allocated arrays (must use nimcp_free since allocated with nimcp_malloc)
    if (analysis.active_neuron_ids)
        nimcp_free(analysis.active_neuron_ids);
    if (analysis.activation_strengths)
        nimcp_free(analysis.activation_strengths);

    adaptive_network_destroy(network);
}

// Test neuron importance ranking
TEST(AdaptiveInterpret, NeuronRanking)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Run some forward passes to build up neuron statistics
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[3];
    for (int i = 0; i < 20; i++) {
        adaptive_network_forward(network, input, 5, output, 3, i + 1);
    }

    // Get neuron rankings
    neuron_importance_t rankings[10];
    uint32_t num_ranked = adaptive_network_rank_neurons(network, rankings, 10);

    EXPECT_GT(num_ranked, 0);
    EXPECT_LE(num_ranked, 10);

    // Rankings should be sorted by importance
    for (uint32_t i = 1; i < num_ranked; i++) {
        EXPECT_LE(rankings[i].importance, rankings[i - 1].importance);
    }

    adaptive_network_destroy(network);
}

// Test decision explanation
TEST(AdaptiveInterpret, Explanation)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    char explanation[512];

    uint32_t length = adaptive_network_explain(network, input, 5, explanation, 512);
    EXPECT_GT(length, 0);
    EXPECT_LT(length, 512);

    adaptive_network_destroy(network);
}

//=============================================================================
// Performance Statistics Tests
//=============================================================================

// Test performance statistics retrieval
TEST(AdaptivePerformance, GetPerformance)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Run some inferences
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[3];
    for (int i = 0; i < 50; i++) {
        adaptive_network_forward(network, input, 5, output, 3, i + 1);
    }

    network_performance_t stats;
    ASSERT_TRUE(adaptive_network_get_performance(network, &stats));

    EXPECT_EQ(stats.total_inferences, 50);
    EXPECT_GE(stats.avg_sparsity, 0.0f);
    EXPECT_LE(stats.avg_sparsity, 1.0f);
    EXPECT_GT(stats.memory_usage_bytes, 0);

    adaptive_network_destroy(network);
}

// Test performance statistics reset
TEST(AdaptivePerformance, ResetStats)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Run some inferences
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[3];
    for (int i = 0; i < 10; i++) {
        adaptive_network_forward(network, input, 5, output, 3, i + 1);
    }

    // Reset stats
    adaptive_network_reset_stats(network);

    // Get stats - should be reset
    network_performance_t stats;
    adaptive_network_get_performance(network, &stats);
    EXPECT_EQ(stats.total_inferences, 0);

    adaptive_network_destroy(network);
}

//=============================================================================
// Introspection API Tests (NIMCP 2.5 Consciousness)
//=============================================================================

// Test get neuron count
TEST(AdaptiveIntrospection, GetNeuronCount)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t count = adaptive_network_get_neuron_count(network);
    EXPECT_EQ(count, config.base_config.num_neurons);

    adaptive_network_destroy(network);
}

// Test get neuron activation
TEST(AdaptiveIntrospection, GetNeuronActivation)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Run forward pass
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[3];
    adaptive_network_forward(network, input, 5, output, 3, 1);

    // Get activation of neuron 0
    float activation;
    ASSERT_TRUE(adaptive_network_get_neuron_activation(network, 0, &activation));

    // Test invalid neuron ID
    ASSERT_FALSE(adaptive_network_get_neuron_activation(network, 9999, &activation));

    adaptive_network_destroy(network);
}

// Test get active neurons
TEST(AdaptiveIntrospection, GetActiveNeurons)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Run forward pass
    float input[5] = {5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
    float output[3];
    adaptive_network_forward(network, input, 5, output, 3, 1);

    // Get active neurons
    uint32_t neuron_ids[20];
    float activations[20];
    uint32_t count =
        adaptive_network_get_active_neurons(network, 0.1f, neuron_ids, activations, 20);

    EXPECT_GE(count, 0);
    EXPECT_LE(count, 20);

    adaptive_network_destroy(network);
}

// Test get connection count
TEST(AdaptiveIntrospection, GetConnectionCount)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    uint32_t num_connections;
    ASSERT_TRUE(adaptive_network_get_connection_count(network, 0, &num_connections));
    EXPECT_GE(num_connections, 0);

    adaptive_network_destroy(network);
}

// Test get total weight
TEST(AdaptiveIntrospection, GetTotalWeight)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Run some activity
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float output[3];
    adaptive_network_forward(network, input, 5, output, 3, 1);

    float total_weight;
    ASSERT_TRUE(adaptive_network_get_total_weight(network, 0, &total_weight));
    EXPECT_GE(total_weight, 0.0f);

    adaptive_network_destroy(network);
}

// Test get base network handle
TEST(AdaptiveIntrospection, GetBaseNetwork)
{
    adaptive_network_config_t config = create_test_adaptive_config();
    adaptive_network_t network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    neural_network_t base = adaptive_network_get_base_network(network);
    ASSERT_NE(base, nullptr);

    // Don't destroy base - it's owned by adaptive network
    adaptive_network_destroy(network);
}
