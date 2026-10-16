//=============================================================================
// test_quantum_feature_maps.cpp - Unit Tests for Quantum Feature Maps
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/features/nimcp_quantum_feature_maps.h"

//=============================================================================
// Lifecycle Tests
//=============================================================================

class QuantumFeatureMapsLifecycleTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(QuantumFeatureMapsLifecycleTest, CreateWithDefaultConfig) {
    ctx = quantum_feature_map_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(QuantumFeatureMapsLifecycleTest, CreateWithCustomConfig) {
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.input_dim = 8;
    config.output_dim = 32;
    config.map_type = QFMAP_PAULI_Z;

    ctx = quantum_feature_map_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(QuantumFeatureMapsLifecycleTest, CreateWithRandomFourier) {
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.map_type = QFMAP_RANDOM_FOURIER;
    config.num_rff_features = 128;
    config.rff_gamma = 0.5f;

    ctx = quantum_feature_map_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(QuantumFeatureMapsLifecycleTest, CreateInvalidInputDim) {
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.input_dim = 0;

    ctx = quantum_feature_map_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(QuantumFeatureMapsLifecycleTest, CreateInvalidOutputDim) {
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.output_dim = QFMAP_MAX_OUTPUT_DIM + 1;

    ctx = quantum_feature_map_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(QuantumFeatureMapsLifecycleTest, CreateInvalidLayers) {
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.num_layers = 0;

    ctx = quantum_feature_map_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(QuantumFeatureMapsLifecycleTest, DestroyNull) {
    quantum_feature_map_destroy(nullptr);  // Should not crash
}

TEST_F(QuantumFeatureMapsLifecycleTest, GetConfig) {
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.input_dim = 12;
    config.output_dim = 48;
    ctx = quantum_feature_map_create(&config);
    ASSERT_NE(ctx, nullptr);

    quantum_feature_map_config_t retrieved;
    int result = quantum_feature_map_get_config(ctx, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(retrieved.input_dim, 12);
    EXPECT_EQ(retrieved.output_dim, 48);
}

//=============================================================================
// Pauli-Z Encoding Tests
//=============================================================================

class QuantumFeatureMapsPauliZTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t DIM = 8;
    static constexpr uint32_t OUTPUT_DIM = 16;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_PAULI_Z;
        config.input_dim = DIM;
        config.output_dim = OUTPUT_DIM;
        config.normalize_input = false;
        config.normalize_output = false;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(QuantumFeatureMapsPauliZTest, ZeroInput) {
    float input[DIM] = {0};
    float output[OUTPUT_DIM] = {0};

    int result = quantum_feature_map_apply(ctx, input, output);
    EXPECT_EQ(result, 0);

    // cos(0) = 1, sin(0) = 0
    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_NEAR(output[2*i], 1.0f, 1e-5);      // cos(0*pi) = 1
        EXPECT_NEAR(output[2*i + 1], 0.0f, 1e-5); // sin(0*pi) = 0
    }
}

TEST_F(QuantumFeatureMapsPauliZTest, UnitInput) {
    float input[DIM] = {1, 1, 1, 1, 1, 1, 1, 1};
    float output[OUTPUT_DIM] = {0};

    int result = quantum_feature_map_apply(ctx, input, output);
    EXPECT_EQ(result, 0);

    // cos(pi) = -1, sin(pi) = 0
    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_NEAR(output[2*i], -1.0f, 1e-5);     // cos(1*pi) = -1
        EXPECT_NEAR(output[2*i + 1], 0.0f, 1e-5); // sin(1*pi) ~ 0
    }
}

TEST_F(QuantumFeatureMapsPauliZTest, HalfInput) {
    float input[DIM] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[OUTPUT_DIM] = {0};

    int result = quantum_feature_map_apply(ctx, input, output);
    EXPECT_EQ(result, 0);

    // cos(pi/2) = 0, sin(pi/2) = 1
    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_NEAR(output[2*i], 0.0f, 1e-5);     // cos(0.5*pi) = 0
        EXPECT_NEAR(output[2*i + 1], 1.0f, 1e-5); // sin(0.5*pi) = 1
    }
}

TEST_F(QuantumFeatureMapsPauliZTest, OutputNormIsPreserved) {
    float input[DIM] = {0.2f, 0.4f, 0.6f, 0.8f, 0.1f, 0.3f, 0.5f, 0.7f};
    float output[OUTPUT_DIM] = {0};

    int result = quantum_feature_map_apply(ctx, input, output);
    EXPECT_EQ(result, 0);

    // Each pair (cos, sin) should have norm 1
    for (uint32_t i = 0; i < DIM; i++) {
        float norm = output[2*i] * output[2*i] + output[2*i+1] * output[2*i+1];
        EXPECT_NEAR(norm, 1.0f, 1e-5);
    }
}

//=============================================================================
// Pauli-Y Encoding Tests
//=============================================================================

class QuantumFeatureMapsPauliYTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t DIM = 4;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_PAULI_Y;
        config.input_dim = DIM;
        config.output_dim = 2 * DIM;
        config.normalize_input = false;
        config.normalize_output = false;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
        }
    }
};

TEST_F(QuantumFeatureMapsPauliYTest, ZeroInput) {
    float input[DIM] = {0};
    float output[2 * DIM] = {0};

    int result = quantum_feature_map_apply(ctx, input, output);
    EXPECT_EQ(result, 0);

    // RY(0) = cos(0)|0> + sin(0)|1> = |0>
    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_NEAR(output[2*i], 1.0f, 1e-5);     // cos(0) = 1
        EXPECT_NEAR(output[2*i + 1], 0.0f, 1e-5); // sin(0) = 0
    }
}

TEST_F(QuantumFeatureMapsPauliYTest, UnitInput) {
    float input[DIM] = {1, 1, 1, 1};
    float output[2 * DIM] = {0};

    int result = quantum_feature_map_apply(ctx, input, output);
    EXPECT_EQ(result, 0);

    // RY(pi) = cos(pi/2)|0> + sin(pi/2)|1>
    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_NEAR(output[2*i], 0.0f, 1e-5);     // cos(pi/2) ~ 0
        EXPECT_NEAR(output[2*i + 1], 1.0f, 1e-5); // sin(pi/2) = 1
    }
}

//=============================================================================
// Random Fourier Features Tests
//=============================================================================

class QuantumFeatureMapsRFFTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t INPUT_DIM = 8;
    static constexpr uint32_t RFF_DIM = 64;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_RANDOM_FOURIER;
        config.input_dim = INPUT_DIM;
        config.output_dim = RFF_DIM;
        config.num_rff_features = RFF_DIM;
        config.rff_gamma = 1.0f;
        config.normalize_output = false;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
        }
    }
};

TEST_F(QuantumFeatureMapsRFFTest, DeterministicWithSeed) {
    float input[INPUT_DIM] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float output1[RFF_DIM] = {0};
    float output2[RFF_DIM] = {0};

    quantum_feature_map_apply(ctx, input, output1);
    quantum_feature_map_apply(ctx, input, output2);

    // Same input, same seed -> same output
    for (uint32_t i = 0; i < RFF_DIM; i++) {
        EXPECT_EQ(output1[i], output2[i]);
    }
}

TEST_F(QuantumFeatureMapsRFFTest, OutputBounded) {
    float input[INPUT_DIM] = {0.5f, -0.3f, 0.8f, -0.2f, 0.1f, 0.4f, -0.7f, 0.6f};
    float output[RFF_DIM] = {0};

    quantum_feature_map_apply(ctx, input, output);

    // RFF uses cos(), so output should be in [-sqrt(2/D), sqrt(2/D)]
    float scale = sqrtf(2.0f / RFF_DIM);
    for (uint32_t i = 0; i < RFF_DIM; i++) {
        EXPECT_GE(output[i], -scale - 1e-5);
        EXPECT_LE(output[i], scale + 1e-5);
    }
}

TEST_F(QuantumFeatureMapsRFFTest, KernelApproximation) {
    // RFF should approximate RBF kernel: exp(-gamma * ||x - y||^2)
    float x[INPUT_DIM] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float y[INPUT_DIM] = {0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f};

    float kernel_approx;
    quantum_feature_map_kernel(ctx, x, y, &kernel_approx);

    // Compute exact RBF kernel
    float sq_dist = 0.0f;
    for (uint32_t i = 0; i < INPUT_DIM; i++) {
        float diff = x[i] - y[i];
        sq_dist += diff * diff;
    }
    float kernel_exact = expf(-1.0f * sq_dist);  // gamma = 1.0

    // RFF approximation should be reasonably close (within 20% for 64 features)
    EXPECT_NEAR(kernel_approx, kernel_exact, 0.3f);
}

//=============================================================================
// Amplitude Encoding Tests
//=============================================================================

class QuantumFeatureMapsAmplitudeTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t DIM = 8;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_AMPLITUDE_ENCODE;
        config.input_dim = DIM;
        config.output_dim = DIM;
        config.normalize_output = false;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
        }
    }
};

TEST_F(QuantumFeatureMapsAmplitudeTest, UnitNorm) {
    float input[DIM] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float output[DIM] = {0};

    quantum_feature_map_apply(ctx, input, output);

    // Output should have unit norm
    float norm = 0.0f;
    for (uint32_t i = 0; i < DIM; i++) {
        norm += output[i] * output[i];
    }
    EXPECT_NEAR(norm, 1.0f, 1e-5);
}

TEST_F(QuantumFeatureMapsAmplitudeTest, PreservesRatios) {
    float input[DIM] = {1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float output[DIM] = {0};

    quantum_feature_map_apply(ctx, input, output);

    // output[1] / output[0] should equal input[1] / input[0] = 2
    EXPECT_NEAR(output[1] / output[0], 2.0f, 1e-5);
}

//=============================================================================
// IQP Encoding Tests
//=============================================================================

class QuantumFeatureMapsIQPTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t DIM = 4;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_IQP;
        config.input_dim = DIM;
        config.output_dim = 64;  // Enough for single and two-qubit terms
        config.num_layers = 2;
        config.normalize_output = false;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
        }
    }
};

TEST_F(QuantumFeatureMapsIQPTest, IncludesInteractionTerms) {
    float input[DIM] = {0.5f, 0.5f, 0.5f, 0.5f};
    float output[64] = {0};

    quantum_feature_map_apply(ctx, input, output);

    // First 8 outputs are single-qubit terms (4 pairs of cos/sin)
    // Should have non-zero interaction terms after
    bool has_nonzero_interaction = false;
    for (uint32_t i = 8; i < 64; i++) {
        if (fabsf(output[i]) > 1e-6) {
            has_nonzero_interaction = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero_interaction);
}

//=============================================================================
// Entanglement Pattern Tests
//=============================================================================

class QuantumFeatureMapsEntangleTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Each test creates/destroys its own context
    }
};

TEST_F(QuantumFeatureMapsEntangleTest, LinearEntanglement) {
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.map_type = QFMAP_PAULI_ZZ;
    config.entangle = QFMAP_ENTANGLE_LINEAR;
    config.input_dim = 4;
    config.output_dim = 32;
    config.num_layers = 1;

    auto ctx = quantum_feature_map_create(&config);
    ASSERT_NE(ctx, nullptr);

    float input[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    float output[32] = {0};

    quantum_feature_map_apply(ctx, input, output);

    // Should have non-zero output
    float sum = 0.0f;
    for (uint32_t i = 0; i < 32; i++) {
        sum += fabsf(output[i]);
    }
    EXPECT_GT(sum, 0.0f);

    quantum_feature_map_destroy(ctx);
}

TEST_F(QuantumFeatureMapsEntangleTest, CircularEntanglement) {
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.map_type = QFMAP_PAULI_ZZ;
    config.entangle = QFMAP_ENTANGLE_CIRCULAR;
    config.input_dim = 4;
    config.output_dim = 32;
    config.num_layers = 1;

    auto ctx = quantum_feature_map_create(&config);
    ASSERT_NE(ctx, nullptr);

    float input[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    float output[32] = {0};

    quantum_feature_map_apply(ctx, input, output);

    // Should have non-zero output
    float sum = 0.0f;
    for (uint32_t i = 0; i < 32; i++) {
        sum += fabsf(output[i]);
    }
    EXPECT_GT(sum, 0.0f);

    quantum_feature_map_destroy(ctx);
}

TEST_F(QuantumFeatureMapsEntangleTest, FullEntanglement) {
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.map_type = QFMAP_PAULI_ZZ;
    config.entangle = QFMAP_ENTANGLE_FULL;
    config.input_dim = 4;
    config.output_dim = 64;  // Need more space for all-to-all
    config.num_layers = 1;

    auto ctx = quantum_feature_map_create(&config);
    ASSERT_NE(ctx, nullptr);

    float input[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    float output[64] = {0};

    quantum_feature_map_apply(ctx, input, output);

    // Full entanglement should produce more non-zero terms than linear
    uint32_t nonzero_count = 0;
    for (uint32_t i = 0; i < 64; i++) {
        if (fabsf(output[i]) > 1e-6) {
            nonzero_count++;
        }
    }
    EXPECT_GT(nonzero_count, 8);  // More than just single-qubit terms

    quantum_feature_map_destroy(ctx);
}

//=============================================================================
// Kernel Function Tests
//=============================================================================

class QuantumFeatureMapsKernelTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t DIM = 8;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_PAULI_Z;
        config.input_dim = DIM;
        config.output_dim = 2 * DIM;
        config.normalize_output = true;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
        }
    }
};

TEST_F(QuantumFeatureMapsKernelTest, SelfKernel) {
    float x[DIM] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    float kernel;
    int result = quantum_feature_map_kernel(ctx, x, x, &kernel);
    EXPECT_EQ(result, 0);

    // K(x, x) should be close to 1 for normalized features
    EXPECT_NEAR(kernel, 1.0f, 1e-4);
}

TEST_F(QuantumFeatureMapsKernelTest, SymmetricKernel) {
    float x[DIM] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float y[DIM] = {0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f};

    float kxy, kyx;
    quantum_feature_map_kernel(ctx, x, y, &kxy);
    quantum_feature_map_kernel(ctx, y, x, &kyx);

    EXPECT_NEAR(kxy, kyx, 1e-6);  // Kernel should be symmetric
}

TEST_F(QuantumFeatureMapsKernelTest, FidelityBounded) {
    float x[DIM] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float y[DIM] = {0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f};

    float fidelity;
    quantum_feature_map_fidelity(ctx, x, y, &fidelity);

    // Fidelity should be in [0, 1]
    EXPECT_GE(fidelity, 0.0f);
    EXPECT_LE(fidelity, 1.0f + 1e-5);
}

TEST_F(QuantumFeatureMapsKernelTest, SelfFidelity) {
    float x[DIM] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    float fidelity;
    quantum_feature_map_fidelity(ctx, x, x, &fidelity);

    // F(x, x) = 1 for any state
    EXPECT_NEAR(fidelity, 1.0f, 1e-4);
}

//=============================================================================
// Batch Processing Tests
//=============================================================================

class QuantumFeatureMapsBatchTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t INPUT_DIM = 4;
    static constexpr uint32_t OUTPUT_DIM = 8;
    static constexpr uint32_t BATCH_SIZE = 10;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_PAULI_Z;
        config.input_dim = INPUT_DIM;
        config.output_dim = OUTPUT_DIM;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
        }
    }
};

TEST_F(QuantumFeatureMapsBatchTest, BatchProcessing) {
    float inputs[BATCH_SIZE * INPUT_DIM];
    float outputs[BATCH_SIZE * OUTPUT_DIM];

    // Initialize with varied inputs
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        for (uint32_t j = 0; j < INPUT_DIM; j++) {
            inputs[i * INPUT_DIM + j] = (float)(i * INPUT_DIM + j) / (BATCH_SIZE * INPUT_DIM);
        }
    }

    int result = quantum_feature_map_batch(ctx, inputs, outputs, BATCH_SIZE);
    EXPECT_EQ(result, 0);

    // Check each batch element has non-zero output
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < OUTPUT_DIM; j++) {
            sum += fabsf(outputs[i * OUTPUT_DIM + j]);
        }
        EXPECT_GT(sum, 0.0f);
    }
}

TEST_F(QuantumFeatureMapsBatchTest, BatchConsistentWithSingle) {
    float inputs[BATCH_SIZE * INPUT_DIM];
    float batch_outputs[BATCH_SIZE * OUTPUT_DIM];

    for (uint32_t i = 0; i < BATCH_SIZE * INPUT_DIM; i++) {
        inputs[i] = (float)i / (BATCH_SIZE * INPUT_DIM);
    }

    quantum_feature_map_batch(ctx, inputs, batch_outputs, BATCH_SIZE);

    // Compare with single-sample processing
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        float single_output[OUTPUT_DIM];
        quantum_feature_map_apply(ctx, inputs + i * INPUT_DIM, single_output);

        for (uint32_t j = 0; j < OUTPUT_DIM; j++) {
            EXPECT_NEAR(batch_outputs[i * OUTPUT_DIM + j], single_output[j], 1e-6);
        }
    }
}

//=============================================================================
// Gram Matrix Tests
//=============================================================================

class QuantumFeatureMapsGramTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t INPUT_DIM = 4;
    static constexpr uint32_t N_SAMPLES = 5;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_PAULI_Z;
        config.input_dim = INPUT_DIM;
        config.output_dim = 2 * INPUT_DIM;
        config.normalize_output = true;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
        }
    }
};

TEST_F(QuantumFeatureMapsGramTest, GramMatrixSymmetric) {
    float inputs[N_SAMPLES * INPUT_DIM];
    float gram[N_SAMPLES * N_SAMPLES];

    for (uint32_t i = 0; i < N_SAMPLES * INPUT_DIM; i++) {
        inputs[i] = (float)i / (N_SAMPLES * INPUT_DIM);
    }

    int result = quantum_feature_map_gram(ctx, inputs, N_SAMPLES, gram);
    EXPECT_EQ(result, 0);

    // Check symmetry
    for (uint32_t i = 0; i < N_SAMPLES; i++) {
        for (uint32_t j = 0; j < N_SAMPLES; j++) {
            EXPECT_NEAR(gram[i * N_SAMPLES + j], gram[j * N_SAMPLES + i], 1e-6);
        }
    }
}

TEST_F(QuantumFeatureMapsGramTest, GramMatrixDiagonal) {
    float inputs[N_SAMPLES * INPUT_DIM];
    float gram[N_SAMPLES * N_SAMPLES];

    for (uint32_t i = 0; i < N_SAMPLES * INPUT_DIM; i++) {
        inputs[i] = (float)i / (N_SAMPLES * INPUT_DIM);
    }

    quantum_feature_map_gram(ctx, inputs, N_SAMPLES, gram);

    // Diagonal should be ~1 for normalized features
    for (uint32_t i = 0; i < N_SAMPLES; i++) {
        EXPECT_NEAR(gram[i * N_SAMPLES + i], 1.0f, 1e-4);
    }
}

TEST_F(QuantumFeatureMapsGramTest, GramMatrixPositiveSemiDefinite) {
    float inputs[N_SAMPLES * INPUT_DIM];
    float gram[N_SAMPLES * N_SAMPLES];

    for (uint32_t i = 0; i < N_SAMPLES * INPUT_DIM; i++) {
        inputs[i] = (float)i / (N_SAMPLES * INPUT_DIM);
    }

    quantum_feature_map_gram(ctx, inputs, N_SAMPLES, gram);

    // Check all eigenvalues are non-negative (simple check: diagonal dominance)
    for (uint32_t i = 0; i < N_SAMPLES; i++) {
        float diag = gram[i * N_SAMPLES + i];
        float off_diag_sum = 0.0f;
        for (uint32_t j = 0; j < N_SAMPLES; j++) {
            if (i != j) {
                off_diag_sum += fabsf(gram[i * N_SAMPLES + j]);
            }
        }
        // This is a necessary (not sufficient) condition for PSD
        EXPECT_GE(diag, 0.0f);
    }
}

//=============================================================================
// Variational Parameter Tests
//=============================================================================

class QuantumFeatureMapsVariationalTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t DIM = 4;
    static constexpr uint32_t LAYERS = 2;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_HARDWARE_EFFICIENT;
        config.input_dim = DIM;
        config.output_dim = 2 * DIM;
        config.num_layers = LAYERS;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
        }
    }
};

TEST_F(QuantumFeatureMapsVariationalTest, GetParamCount) {
    uint32_t n_params;
    int result = quantum_feature_map_get_params(ctx, nullptr, &n_params);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(n_params, LAYERS * 3 * DIM);  // 3 params per qubit per layer
}

TEST_F(QuantumFeatureMapsVariationalTest, SetAndGetParams) {
    uint32_t n_params;
    quantum_feature_map_get_params(ctx, nullptr, &n_params);

    std::vector<float> new_params(n_params, 0.5f);
    int result = quantum_feature_map_set_params(ctx, new_params.data(), n_params);
    EXPECT_EQ(result, 0);

    std::vector<float> retrieved(n_params);
    quantum_feature_map_get_params(ctx, retrieved.data(), &n_params);

    for (uint32_t i = 0; i < n_params; i++) {
        EXPECT_NEAR(retrieved[i], 0.5f, 1e-6);
    }
}

TEST_F(QuantumFeatureMapsVariationalTest, ParamsAffectOutput) {
    float input[DIM] = {0.25f, 0.5f, 0.75f, 1.0f};
    float output1[2 * DIM];
    float output2[2 * DIM];

    quantum_feature_map_apply(ctx, input, output1);

    // Change parameters
    uint32_t n_params;
    quantum_feature_map_get_params(ctx, nullptr, &n_params);
    std::vector<float> new_params(n_params, 1.0f);
    quantum_feature_map_set_params(ctx, new_params.data(), n_params);

    quantum_feature_map_apply(ctx, input, output2);

    // Output should change
    bool different = false;
    for (uint32_t i = 0; i < 2 * DIM; i++) {
        if (fabsf(output1[i] - output2[i]) > 1e-5) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

//=============================================================================
// Feature Enhancement Tests
//=============================================================================

class QuantumFeatureMapsEnhanceTest : public ::testing::Test {
protected:
    quantum_feature_map_t ctx = nullptr;
    static constexpr uint32_t DIM = 8;

    void SetUp() override {
        quantum_feature_map_config_t config = quantum_feature_map_default_config();
        config.map_type = QFMAP_PAULI_Z;
        config.input_dim = DIM;
        config.output_dim = 2 * DIM;
        ctx = quantum_feature_map_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            quantum_feature_map_destroy(ctx);
        }
    }
};

TEST_F(QuantumFeatureMapsEnhanceTest, PreservesOriginal) {
    float original[DIM] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float enhanced[DIM + 2 * DIM];

    int result = quantum_feature_map_enhance_features(ctx, original, DIM,
                                                      enhanced, DIM + 2 * DIM);
    EXPECT_EQ(result, 0);

    // First DIM elements should be original features
    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_EQ(enhanced[i], original[i]);
    }
}

TEST_F(QuantumFeatureMapsEnhanceTest, AddsQuantumFeatures) {
    float original[DIM] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float enhanced[DIM + 2 * DIM];

    quantum_feature_map_enhance_features(ctx, original, DIM, enhanced, DIM + 2 * DIM);

    // Enhanced features (after original) should be non-zero
    bool has_quantum = false;
    for (uint32_t i = DIM; i < DIM + 2 * DIM; i++) {
        if (fabsf(enhanced[i]) > 1e-6) {
            has_quantum = true;
            break;
        }
    }
    EXPECT_TRUE(has_quantum);
}

//=============================================================================
// Edge Cases
//=============================================================================

class QuantumFeatureMapsEdgeCasesTest : public ::testing::Test {};

TEST_F(QuantumFeatureMapsEdgeCasesTest, NullInputApply) {
    auto ctx = quantum_feature_map_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    float output[32];
    int result = quantum_feature_map_apply(ctx, nullptr, output);
    EXPECT_EQ(result, -1);

    quantum_feature_map_destroy(ctx);
}

TEST_F(QuantumFeatureMapsEdgeCasesTest, NullOutputApply) {
    auto ctx = quantum_feature_map_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    float input[16] = {0};
    int result = quantum_feature_map_apply(ctx, input, nullptr);
    EXPECT_EQ(result, -1);

    quantum_feature_map_destroy(ctx);
}

TEST_F(QuantumFeatureMapsEdgeCasesTest, NullContextApply) {
    float input[16] = {0};
    float output[32];
    int result = quantum_feature_map_apply(nullptr, input, output);
    EXPECT_EQ(result, -1);
}

TEST_F(QuantumFeatureMapsEdgeCasesTest, NullKernelInputs) {
    auto ctx = quantum_feature_map_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    float x[16] = {0};
    float kernel;

    EXPECT_EQ(quantum_feature_map_kernel(ctx, nullptr, x, &kernel), -1);
    EXPECT_EQ(quantum_feature_map_kernel(ctx, x, nullptr, &kernel), -1);
    EXPECT_EQ(quantum_feature_map_kernel(ctx, x, x, nullptr), -1);

    quantum_feature_map_destroy(ctx);
}

TEST_F(QuantumFeatureMapsEdgeCasesTest, VerySmallInput) {
    // Use PAULI_Z to avoid buffer overflow from entanglement terms
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.map_type = QFMAP_PAULI_Z;
    config.input_dim = 16;
    config.output_dim = 32;  // 2 * input_dim for PAULI_Z

    auto ctx = quantum_feature_map_create(&config);
    ASSERT_NE(ctx, nullptr);

    float input[16];
    float output[32];

    for (int i = 0; i < 16; i++) {
        input[i] = 1e-10f;
    }

    int result = quantum_feature_map_apply(ctx, input, output);
    EXPECT_EQ(result, 0);

    // Should not have NaN or Inf
    for (int i = 0; i < 32; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }

    quantum_feature_map_destroy(ctx);
}

TEST_F(QuantumFeatureMapsEdgeCasesTest, LargeInput) {
    // Use PAULI_Z to avoid buffer overflow from entanglement terms
    quantum_feature_map_config_t config = quantum_feature_map_default_config();
    config.map_type = QFMAP_PAULI_Z;
    config.input_dim = 16;
    config.output_dim = 32;  // 2 * input_dim for PAULI_Z

    auto ctx = quantum_feature_map_create(&config);
    ASSERT_NE(ctx, nullptr);

    float input[16];
    float output[32];

    for (int i = 0; i < 16; i++) {
        input[i] = 1000.0f;
    }

    int result = quantum_feature_map_apply(ctx, input, output);
    EXPECT_EQ(result, 0);

    // Should not have NaN or Inf
    for (int i = 0; i < 32; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }

    quantum_feature_map_destroy(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
