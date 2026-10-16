/**
 * @file test_cognitive_gpu_recovery.cpp
 * @brief Integration tests for GPU recovery in cognitive modules
 *
 * WHAT: Integration tests for GPU recovery across all cognitive modules
 * WHY:  Verify GPU recovery works correctly with Broca, Wernicke, JEPA, Omni, Occipital
 * HOW:  Test recovery from OOM, numerical errors, context invalidation, and CPU fallback
 *
 * @version 1.0
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>

// GPU headers
#include "gpu/cognitive/nimcp_broca_gpu.h"
#include "gpu/cognitive/nimcp_wernicke_gpu.h"
#include "gpu/cognitive/nimcp_jepa_gpu.h"
#include "gpu/cognitive/nimcp_omni_gpu.h"
#include "gpu/occipital/nimcp_occipital_gpu_bridge.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveGPURecoveryIntegrationTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx;
    std::mt19937 rng;

    // Contexts for different modules
    broca_gpu_context_t* broca_ctx;
    wernicke_gpu_context_t* wernicke_ctx;
    nimcp_jepa_gpu_predictor_t* jepa_predictor;
    nimcp_omni_gpu_state_t* omni_state;
    occipital_adapter_t* occipital_adapter;
    occipital_gpu_bridge_t* occipital_bridge;

    void SetUp() override {
        gpu_ctx = nullptr;
        broca_ctx = nullptr;
        wernicke_ctx = nullptr;
        jepa_predictor = nullptr;
        omni_state = nullptr;
        occipital_adapter = nullptr;
        occipital_bridge = nullptr;
        rng.seed(42);

        // Initialize kernel backend
        nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);

        // Create GPU context
        if (nimcp_cuda_backend_available()) {
            gpu_ctx = nimcp_gpu_context_create(0);
        }

        if (gpu_ctx) {
            // Create all cognitive contexts
            broca_gpu_config_t broca_config = broca_gpu_default_config();
            broca_ctx = broca_gpu_create(gpu_ctx, &broca_config);

            wernicke_gpu_config_t wernicke_config = wernicke_gpu_default_config();
            wernicke_ctx = wernicke_gpu_create(gpu_ctx, &wernicke_config);

            jepa_predictor = nimcp_jepa_gpu_predictor_create(
                gpu_ctx, 64, 128, 64, 2, NIMCP_JEPA_ACT_GELU
            );

            omni_state = nimcp_omni_gpu_create(gpu_ctx, 128, 256, 100, 4);

            occipital_adapter = occipital_create(nullptr);
            if (occipital_adapter) {
                occipital_gpu_bridge_config_t bridge_config = occipital_gpu_bridge_default_config();
                occipital_bridge = occipital_gpu_bridge_create(occipital_adapter, &bridge_config);
            }
        }
    }

    void TearDown() override {
        if (occipital_bridge) {
            occipital_gpu_bridge_destroy(occipital_bridge);
            occipital_bridge = nullptr;
        }
        if (occipital_adapter) {
            occipital_destroy(occipital_adapter);
            occipital_adapter = nullptr;
        }
        if (omni_state) {
            nimcp_omni_gpu_destroy(omni_state);
            omni_state = nullptr;
        }
        if (jepa_predictor) {
            nimcp_jepa_gpu_predictor_destroy(jepa_predictor);
            jepa_predictor = nullptr;
        }
        if (wernicke_ctx) {
            wernicke_gpu_destroy(wernicke_ctx);
            wernicke_ctx = nullptr;
        }
        if (broca_ctx) {
            broca_gpu_destroy(broca_ctx);
            broca_ctx = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
        nimcp_kernel_backend_shutdown();
    }

    bool hasGPU() const {
        return gpu_ctx != nullptr;
    }

    bool hasAllModules() const {
        return broca_ctx && wernicke_ctx && jepa_predictor &&
               omni_state && occipital_bridge;
    }

    std::vector<float> generateRandomVector(uint32_t size) {
        std::vector<float> vec(size);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& v : vec) v = dist(rng);
        return vec;
    }

    nimcp_gpu_tensor_t* createTensor(const std::vector<float>& data, uint32_t rows, uint32_t cols) {
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(gpu_ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_tensor_upload(tensor, data.data(), data.size() * sizeof(float));
        }
        return tensor;
    }
};

//=============================================================================
// Recovery System Initialization Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, Recovery_InitializedForAllModules) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Recovery should be initialized when any cognitive module is created
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
}

TEST_F(CognitiveGPURecoveryIntegrationTest, Recovery_SharedAcrossModules) {
    if (!hasGPU() || !hasAllModules()) {
        GTEST_SKIP() << "GPU or modules not available";
    }

    // All modules should share the same recovery system
    // Recovery init should only happen once
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    // Operations on any module should work
    broca_gpu_synchronize(broca_ctx);
    wernicke_gpu_synchronize(wernicke_ctx);
    nimcp_jepa_gpu_synchronize(gpu_ctx);
    nimcp_omni_gpu_synchronize(omni_state);
}

//=============================================================================
// Broca GPU Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, Broca_RecoveryFromNullInput) {
    if (!hasGPU() || !broca_ctx) {
        GTEST_SKIP() << "GPU or Broca not available";
    }

    // These should fail gracefully with recovery
    EXPECT_FALSE(broca_gpu_batch_lexical_lookup(broca_ctx, nullptr, 0, nullptr));
    EXPECT_FALSE(broca_gpu_encode_phonemes(broca_ctx, nullptr, 0, nullptr, 0, nullptr, nullptr));
    EXPECT_FALSE(broca_gpu_generate_motor_commands(broca_ctx, nullptr, 0, nullptr, 0, nullptr, 0.0f));

    // Context should still be valid after recovery
    EXPECT_TRUE(broca_gpu_synchronize(broca_ctx));
}

TEST_F(CognitiveGPURecoveryIntegrationTest, Broca_ContinuesAfterRecovery) {
    if (!hasGPU() || !broca_ctx) {
        GTEST_SKIP() << "GPU or Broca not available";
    }

    // Create valid test data
    broca_gpu_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.word_id = 1;
    entry.phonemes[0] = 'h'; entry.phonemes[1] = 'e';
    entry.phoneme_count = 2;
    entry.frequency = 0.9f;

    // Upload lexicon
    EXPECT_TRUE(broca_gpu_upload_lexicon(broca_ctx, &entry, 1));

    // Trigger a recoverable error with null
    broca_gpu_batch_lexical_lookup(broca_ctx, nullptr, 0, nullptr);

    // Should still work after recovery
    uint32_t word_ids[] = {1};
    broca_gpu_lookup_result_t results[1];
    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_ctx, word_ids, 1, results));
    EXPECT_TRUE(results[0].found);
}

//=============================================================================
// Wernicke GPU Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, Wernicke_RecoveryFromNullInput) {
    if (!hasGPU() || !wernicke_ctx) {
        GTEST_SKIP() << "GPU or Wernicke not available";
    }

    EXPECT_FALSE(wernicke_gpu_recognize_phonemes(wernicke_ctx, nullptr, 0, nullptr));
    EXPECT_FALSE(wernicke_gpu_recognize_words(wernicke_ctx, nullptr, 0, nullptr, 0, nullptr));
    EXPECT_FALSE(wernicke_gpu_spread_activation(wernicke_ctx, nullptr, nullptr, 0, nullptr, 0, nullptr));

    // Context should still be valid
    EXPECT_TRUE(wernicke_gpu_synchronize(wernicke_ctx));
}

TEST_F(CognitiveGPURecoveryIntegrationTest, Wernicke_RecoveryDuringComprehension) {
    if (!hasGPU() || !wernicke_ctx) {
        GTEST_SKIP() << "GPU or Wernicke not available";
    }

    // Upload lexicon
    wernicke_gpu_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.word_id = 1;
    entry.phonemes[0] = 8; entry.phonemes[1] = 5;
    entry.phoneme_count = 2;
    entry.frequency = 0.9f;
    wernicke_gpu_upload_lexicon(wernicke_ctx, &entry, 1);

    // Try to cause an error
    wernicke_gpu_recognize_words(wernicke_ctx, nullptr, 0, nullptr, 0, nullptr);

    // Continue with valid input
    uint8_t phonemes[] = {8, 5};
    wernicke_gpu_word_candidate_t candidates[10];
    uint32_t num = 0;
    EXPECT_TRUE(wernicke_gpu_recognize_words(wernicke_ctx, phonemes, 2, candidates, 10, &num));
}

//=============================================================================
// JEPA GPU Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, JEPA_RecoveryFromNullTensors) {
    if (!hasGPU() || !jepa_predictor) {
        GTEST_SKIP() << "GPU or JEPA not available";
    }

    EXPECT_FALSE(nimcp_jepa_gpu_forward_predict(jepa_predictor, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_backward(jepa_predictor, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_compute_loss(gpu_ctx, nullptr, nullptr, nullptr, nullptr));

    EXPECT_TRUE(nimcp_jepa_gpu_synchronize(gpu_ctx));
}

TEST_F(CognitiveGPURecoveryIntegrationTest, JEPA_RecoveryFromNumericalIssues) {
    if (!hasGPU() || !jepa_predictor) {
        GTEST_SKIP() << "GPU or JEPA not available";
    }

    // Upload random weights
    for (uint32_t layer = 0; layer < 2; layer++) {
        uint32_t in_dim = (layer == 0) ? 64 : 128;
        uint32_t out_dim = (layer == 1) ? 64 : 128;
        auto weights = generateRandomVector(out_dim * in_dim);
        auto bias = generateRandomVector(out_dim);
        nimcp_jepa_gpu_predictor_upload_weights(jepa_predictor, layer, weights.data(), bias.data());
    }

    // Test with very large values (potential numerical issues)
    std::vector<float> large_input(8 * 64, 1e10f);
    nimcp_gpu_tensor_t* input = createTensor(large_input, 8, 64);

    size_t out_dims[2] = {8, 64};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    // This might produce NaN/Inf, but should recover
    bool result = nimcp_jepa_gpu_forward_predict(jepa_predictor, input, output);

    // Even if it fails, context should remain valid
    EXPECT_TRUE(nimcp_jepa_gpu_synchronize(gpu_ctx));

    // Try again with normal values
    auto normal_input = generateRandomVector(8 * 64);
    nimcp_gpu_tensor_t* normal = createTensor(normal_input, 8, 64);
    EXPECT_TRUE(nimcp_jepa_gpu_forward_predict(jepa_predictor, normal, output));

    nimcp_gpu_tensor_destroy(normal);
    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(input);
}

//=============================================================================
// Omni GPU Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, Omni_RecoveryFromNullInput) {
    if (!hasGPU() || !omni_state) {
        GTEST_SKIP() << "GPU or Omni not available";
    }

    EXPECT_FALSE(nimcp_omni_gpu_predict(omni_state, nullptr, nullptr, NIMCP_OMNI_GPU_DIR_FORWARD));
    EXPECT_FALSE(nimcp_omni_gpu_hopfield_retrieve(omni_state, nullptr, nullptr, 10));
    EXPECT_FALSE(nimcp_omni_gpu_hierarchy_forward(omni_state, nullptr));
    EXPECT_FALSE(nimcp_omni_gpu_compute_free_energy(omni_state, nullptr, nullptr, nullptr));

    EXPECT_TRUE(nimcp_omni_gpu_synchronize(omni_state));
}

TEST_F(CognitiveGPURecoveryIntegrationTest, Omni_HopfieldRecoveryAfterError) {
    if (!hasGPU() || !omni_state) {
        GTEST_SKIP() << "GPU or Omni not available";
    }

    // Initialize Hopfield
    ASSERT_TRUE(nimcp_omni_gpu_hopfield_init(
        omni_state, 128, 100, 1.0f, NIMCP_HOPFIELD_GPU_SOFTMAX
    ));

    // Store a valid pattern
    auto pattern_data = generateRandomVector(128);
    nimcp_gpu_tensor_t* pattern = createTensor(pattern_data, 1, 128);
    int idx = nimcp_omni_gpu_hopfield_store(omni_state, pattern);
    EXPECT_GE(idx, 0);

    // Trigger error
    nimcp_omni_gpu_hopfield_retrieve(omni_state, nullptr, nullptr, 10);

    // Should still be able to retrieve
    size_t out_dims[2] = {1, 128};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    EXPECT_TRUE(nimcp_omni_gpu_hopfield_retrieve(omni_state, pattern, output, 10));

    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(pattern);
}

TEST_F(CognitiveGPURecoveryIntegrationTest, Omni_ReplayRecoveryAfterError) {
    if (!hasGPU() || !omni_state) {
        GTEST_SKIP() << "GPU or Omni not available";
    }

    // Initialize replay buffer
    ASSERT_TRUE(nimcp_omni_gpu_replay_init(omni_state, 128, 50, 16));

    // Store a sequence
    auto sequence_data = generateRandomVector(16 * 128);
    nimcp_gpu_tensor_t* sequence = createTensor(sequence_data, 16, 128);
    int idx = nimcp_omni_gpu_replay_store(omni_state, sequence, 1.0f);
    EXPECT_GE(idx, 0);

    // Trigger error with null sampling
    nimcp_omni_gpu_replay_sample(omni_state, 0, NIMCP_REPLAY_GPU_PRIORITY, nullptr, nullptr);

    // Should still be able to sample
    size_t sample_dims[3] = {4, 16, 128};
    nimcp_gpu_tensor_t* sampled = nimcp_gpu_tensor_create(gpu_ctx, sample_dims, 3, NIMCP_GPU_PRECISION_FP32);
    std::vector<uint32_t> indices(4);
    EXPECT_TRUE(nimcp_omni_gpu_replay_sample(
        omni_state, 4, NIMCP_REPLAY_GPU_FORWARD, sampled, indices.data()
    ));

    nimcp_gpu_tensor_destroy(sampled);
    nimcp_gpu_tensor_destroy(sequence);
}

//=============================================================================
// Occipital GPU Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, Occipital_RecoveryFromNullInput) {
    if (!hasGPU() || !occipital_bridge) {
        GTEST_SKIP() << "GPU or Occipital not available";
    }

    EXPECT_FALSE(occipital_gpu_upload_input(occipital_bridge, nullptr));
    EXPECT_FALSE(occipital_gpu_download_features(occipital_bridge, nullptr, 0, nullptr));
    EXPECT_FALSE(occipital_gpu_process(occipital_bridge, nullptr));

    EXPECT_TRUE(occipital_gpu_bridge_is_available(occipital_bridge));
}

TEST_F(CognitiveGPURecoveryIntegrationTest, Occipital_CPUFallbackWorks) {
    if (!hasGPU() || !occipital_bridge) {
        GTEST_SKIP() << "GPU or Occipital not available";
    }

    // Configure with auto-fallback
    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    config.auto_fallback = true;
    occipital_gpu_bridge_configure(occipital_bridge, &config);

    // Create valid input
    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = 64;
    input.height = 64;
    input.channels = 3;
    input.stride = input.width * input.channels;
    input.data = (uint8_t*)malloc(input.width * input.height * input.channels);
    for (size_t i = 0; i < input.width * input.height * input.channels; i++) {
        input.data[i] = (uint8_t)(rng() % 256);
    }

    visual_processing_result_t result;
    EXPECT_TRUE(occipital_gpu_process_input(occipital_bridge, &input, &result));

    free(input.data);
}

//=============================================================================
// Cross-Module Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, CrossModule_RecoveryIsolated) {
    if (!hasGPU() || !hasAllModules()) {
        GTEST_SKIP() << "GPU or modules not available";
    }

    // Trigger error in Broca
    broca_gpu_batch_lexical_lookup(broca_ctx, nullptr, 0, nullptr);

    // Other modules should still work
    EXPECT_TRUE(wernicke_gpu_synchronize(wernicke_ctx));
    EXPECT_TRUE(nimcp_jepa_gpu_synchronize(gpu_ctx));
    EXPECT_TRUE(nimcp_omni_gpu_synchronize(omni_state));
    EXPECT_TRUE(occipital_gpu_bridge_is_available(occipital_bridge));
}

TEST_F(CognitiveGPURecoveryIntegrationTest, CrossModule_SequentialRecovery) {
    if (!hasGPU() || !hasAllModules()) {
        GTEST_SKIP() << "GPU or modules not available";
    }

    // Trigger errors in each module
    broca_gpu_batch_lexical_lookup(broca_ctx, nullptr, 0, nullptr);
    wernicke_gpu_recognize_phonemes(wernicke_ctx, nullptr, 0, nullptr);
    nimcp_jepa_gpu_forward_predict(jepa_predictor, nullptr, nullptr);
    nimcp_omni_gpu_predict(omni_state, nullptr, nullptr, NIMCP_OMNI_GPU_DIR_FORWARD);
    occipital_gpu_upload_input(occipital_bridge, nullptr);

    // All should recover
    EXPECT_TRUE(broca_gpu_synchronize(broca_ctx));
    EXPECT_TRUE(wernicke_gpu_synchronize(wernicke_ctx));
    EXPECT_TRUE(nimcp_jepa_gpu_synchronize(gpu_ctx));
    EXPECT_TRUE(nimcp_omni_gpu_synchronize(omni_state));
    EXPECT_TRUE(occipital_gpu_bridge_is_available(occipital_bridge));
}

TEST_F(CognitiveGPURecoveryIntegrationTest, CrossModule_SharedContextStability) {
    if (!hasGPU() || !hasAllModules()) {
        GTEST_SKIP() << "GPU or modules not available";
    }

    // All modules share the same GPU context
    // Operations on one should not corrupt another

    // Setup Broca with lexicon
    broca_gpu_lexical_entry_t broca_entry;
    memset(&broca_entry, 0, sizeof(broca_entry));
    broca_entry.word_id = 1;
    broca_entry.phonemes[0] = 't'; broca_entry.phonemes[1] = 'e';
    broca_entry.phoneme_count = 2;
    broca_gpu_upload_lexicon(broca_ctx, &broca_entry, 1);

    // Setup Wernicke with lexicon
    wernicke_gpu_lexical_entry_t wernicke_entry;
    memset(&wernicke_entry, 0, sizeof(wernicke_entry));
    wernicke_entry.word_id = 1;
    wernicke_entry.phonemes[0] = 20; wernicke_entry.phonemes[1] = 5;
    wernicke_entry.phoneme_count = 2;
    wernicke_gpu_upload_lexicon(wernicke_ctx, &wernicke_entry, 1);

    // Trigger errors
    nimcp_jepa_gpu_forward_predict(jepa_predictor, nullptr, nullptr);
    nimcp_omni_gpu_predict(omni_state, nullptr, nullptr, NIMCP_OMNI_GPU_DIR_FORWARD);

    // Verify Broca and Wernicke lexicons are still valid
    EXPECT_EQ(broca_gpu_get_lexicon_size(broca_ctx), 1u);
    EXPECT_EQ(wernicke_gpu_get_lexicon_size(wernicke_ctx), 1u);
}

//=============================================================================
// Language Processing Pipeline Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, LanguagePipeline_RecoveryDuringProduction) {
    if (!hasGPU() || !broca_ctx) {
        GTEST_SKIP() << "GPU or Broca not available";
    }

    // Setup Broca
    std::vector<broca_gpu_lexical_entry_t> lexicon;
    broca_gpu_lexical_entry_t entry;

    memset(&entry, 0, sizeof(entry));
    entry.word_id = 1;
    entry.phonemes[0] = 'h'; entry.phonemes[1] = 'e';
    entry.phonemes[2] = 'l'; entry.phonemes[3] = 'o';
    entry.phoneme_count = 4;
    entry.frequency = 0.9f;
    lexicon.push_back(entry);

    broca_gpu_upload_lexicon(broca_ctx, lexicon.data(), lexicon.size());

    // Run pipeline
    uint32_t word_ids[] = {1};
    broca_gpu_lookup_result_t results[1];
    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_ctx, word_ids, 1, results));

    // Trigger mid-pipeline error
    broca_gpu_encode_phonemes(broca_ctx, nullptr, 0, nullptr, 0, nullptr, nullptr);

    // Continue pipeline
    uint8_t phonemes[16];
    uint32_t phoneme_count = 0;
    uint32_t boundaries[1];
    EXPECT_TRUE(broca_gpu_encode_phonemes(
        broca_ctx, word_ids, 1, phonemes, 16, &phoneme_count, boundaries
    ));

    broca_gpu_motor_command_t commands[64];
    uint32_t command_count = 0;
    EXPECT_TRUE(broca_gpu_generate_motor_commands(
        broca_ctx, phonemes, phoneme_count, commands, 64, &command_count, 0.0f
    ));
}

TEST_F(CognitiveGPURecoveryIntegrationTest, LanguagePipeline_RecoveryDuringComprehension) {
    if (!hasGPU() || !wernicke_ctx) {
        GTEST_SKIP() << "GPU or Wernicke not available";
    }

    // Setup Wernicke
    wernicke_gpu_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.word_id = 1;
    entry.phonemes[0] = 8; entry.phonemes[1] = 5;
    entry.phoneme_count = 2;
    entry.frequency = 0.9f;
    entry.concept_id = 100;
    wernicke_gpu_upload_lexicon(wernicke_ctx, &entry, 1);

    // Process phonemes
    uint8_t phonemes[] = {8, 5};
    wernicke_gpu_word_candidate_t candidates[10];
    uint32_t num_candidates = 0;

    EXPECT_TRUE(wernicke_gpu_recognize_words(
        wernicke_ctx, phonemes, 2, candidates, 10, &num_candidates
    ));

    // Trigger mid-pipeline error
    wernicke_gpu_spread_activation(wernicke_ctx, nullptr, nullptr, 0, nullptr, 0, nullptr);

    // Continue with semantic activation
    uint32_t seeds[] = {100};
    float seed_activations[] = {1.0f};
    wernicke_gpu_activation_result_t activations[50];
    uint32_t num_activations = 0;

    EXPECT_TRUE(wernicke_gpu_spread_activation(
        wernicke_ctx, seeds, seed_activations, 1, activations, 50, &num_activations
    ));
}

//=============================================================================
// Prediction Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, Prediction_JEPARecoveryMidTraining) {
    if (!hasGPU() || !jepa_predictor) {
        GTEST_SKIP() << "GPU or JEPA not available";
    }

    // Upload weights
    for (uint32_t layer = 0; layer < 2; layer++) {
        uint32_t in_dim = (layer == 0) ? 64 : 128;
        uint32_t out_dim = (layer == 1) ? 64 : 128;
        auto weights = generateRandomVector(out_dim * in_dim);
        auto bias = generateRandomVector(out_dim);
        nimcp_jepa_gpu_predictor_upload_weights(jepa_predictor, layer, weights.data(), bias.data());
    }

    // Forward pass
    auto input_data = generateRandomVector(8 * 64);
    nimcp_gpu_tensor_t* input = createTensor(input_data, 8, 64);

    size_t out_dims[2] = {8, 64};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_forward_predict(jepa_predictor, input, output));

    // Trigger error during backward
    nimcp_jepa_gpu_backward(jepa_predictor, nullptr, nullptr);

    // Should still be able to do another forward
    EXPECT_TRUE(nimcp_jepa_gpu_forward_predict(jepa_predictor, input, output));

    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(input);
}

TEST_F(CognitiveGPURecoveryIntegrationTest, Prediction_OmniRecoveryDuringInference) {
    if (!hasGPU() || !omni_state) {
        GTEST_SKIP() << "GPU or Omni not available";
    }

    // Upload weights
    auto weights = generateRandomVector(128 * 128);
    auto bias = generateRandomVector(128);
    nimcp_omni_gpu_upload_weights(omni_state, NIMCP_OMNI_GPU_DIR_FORWARD, weights.data(), bias.data());

    auto input_data = generateRandomVector(8 * 128);
    nimcp_gpu_tensor_t* input = createTensor(input_data, 8, 128);

    size_t out_dims[2] = {8, 128};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    // Multi-direction prediction
    EXPECT_TRUE(nimcp_omni_gpu_predict(omni_state, input, output, NIMCP_OMNI_GPU_DIR_FORWARD));

    // Trigger error
    nimcp_omni_gpu_predict(omni_state, nullptr, nullptr, NIMCP_OMNI_GPU_DIR_BACKWARD);

    // Continue with other direction
    nimcp_omni_gpu_upload_weights(omni_state, NIMCP_OMNI_GPU_DIR_BACKWARD, weights.data(), bias.data());
    EXPECT_TRUE(nimcp_omni_gpu_predict(omni_state, input, output, NIMCP_OMNI_GPU_DIR_BACKWARD));

    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(input);
}

//=============================================================================
// Visual Processing Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, Visual_RecoveryDuringPipeline) {
    if (!hasGPU() || !occipital_bridge) {
        GTEST_SKIP() << "GPU or Occipital not available";
    }

    occipital_gpu_bridge_init_size(occipital_bridge, 64, 64, 3);

    // Create valid input
    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = 64;
    input.height = 64;
    input.channels = 3;
    input.stride = input.width * input.channels;
    input.data = (uint8_t*)malloc(input.width * input.height * input.channels);
    for (size_t i = 0; i < input.width * input.height * input.channels; i++) {
        input.data[i] = (uint8_t)(rng() % 256);
    }

    EXPECT_TRUE(occipital_gpu_upload_input(occipital_bridge, &input));
    EXPECT_TRUE(occipital_gpu_process_v1(occipital_bridge));

    // Trigger error mid-pipeline
    occipital_gpu_download_features(occipital_bridge, nullptr, 0, nullptr);

    // Continue pipeline
    EXPECT_TRUE(occipital_gpu_process_v4(occipital_bridge));
    EXPECT_TRUE(occipital_gpu_compute_saliency(occipital_bridge));

    free(input.data);
}

//=============================================================================
// Stress Recovery Tests
//=============================================================================

TEST_F(CognitiveGPURecoveryIntegrationTest, Stress_MultipleConsecutiveErrors) {
    if (!hasGPU() || !hasAllModules()) {
        GTEST_SKIP() << "GPU or modules not available";
    }

    // Trigger many errors in sequence
    for (int i = 0; i < 10; i++) {
        broca_gpu_batch_lexical_lookup(broca_ctx, nullptr, 0, nullptr);
        wernicke_gpu_recognize_phonemes(wernicke_ctx, nullptr, 0, nullptr);
        nimcp_jepa_gpu_forward_predict(jepa_predictor, nullptr, nullptr);
        nimcp_omni_gpu_predict(omni_state, nullptr, nullptr, NIMCP_OMNI_GPU_DIR_FORWARD);
        occipital_gpu_upload_input(occipital_bridge, nullptr);
    }

    // All should still work
    EXPECT_TRUE(broca_gpu_synchronize(broca_ctx));
    EXPECT_TRUE(wernicke_gpu_synchronize(wernicke_ctx));
    EXPECT_TRUE(nimcp_jepa_gpu_synchronize(gpu_ctx));
    EXPECT_TRUE(nimcp_omni_gpu_synchronize(omni_state));
    EXPECT_TRUE(occipital_gpu_bridge_is_available(occipital_bridge));
}

TEST_F(CognitiveGPURecoveryIntegrationTest, Stress_AlternatingValidAndInvalidOps) {
    if (!hasGPU() || !broca_ctx) {
        GTEST_SKIP() << "GPU or Broca not available";
    }

    // Setup
    broca_gpu_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.word_id = 1;
    entry.phonemes[0] = 'a';
    entry.phoneme_count = 1;
    broca_gpu_upload_lexicon(broca_ctx, &entry, 1);

    uint32_t word_ids[] = {1};
    broca_gpu_lookup_result_t results[1];

    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            // Valid operation
            EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_ctx, word_ids, 1, results));
        } else {
            // Invalid operation (should recover)
            broca_gpu_batch_lexical_lookup(broca_ctx, nullptr, 0, nullptr);
        }
    }

    // Final valid operation should still work
    EXPECT_TRUE(broca_gpu_batch_lexical_lookup(broca_ctx, word_ids, 1, results));
    EXPECT_TRUE(results[0].found);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
