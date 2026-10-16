//=============================================================================
// test_fuzzy_gpu_anfis.cpp - Integration Tests for GPU ANFIS Training
//=============================================================================
/**
 * @file test_fuzzy_gpu_anfis.cpp
 * @brief Integration tests for GPU-accelerated ANFIS (Adaptive Neuro-Fuzzy
 *        Inference System) training and fuzzy relation composition
 *
 * Tests ANFIS training convergence, parameter learning, relation operations,
 * and CPU-GPU parameter synchronization.
 *
 * COVERAGE:
 *   - ANFIS training on GPU
 *   - Training convergence
 *   - Parameter download to CPU
 *   - Relation composition
 *   - Batch relation operations
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_params.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"
#include "utils/fuzzy/nimcp_fuzzy_mf.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FuzzyGPUANFISTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    fuzzy_inference_engine_t* cpu_engine = nullptr;

    void SetUp() override {
        ctx = nimcp_gpu_context_create(0);
        if (!ctx) {
            GTEST_SKIP() << "No GPU available - skipping test";
        }
    }

    void TearDown() override {
        if (cpu_engine) {
            fuzzy_inference_destroy(cpu_engine);
            cpu_engine = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Create a simple Sugeno FIS for function approximation
    fuzzy_inference_engine_t* createSugenoApproximator() {
        fuzzy_inference_config_t config = fuzzy_inference_default_config();
        config.fis_type = FUZZY_FIS_SUGENO;
        config.and_method = FUZZY_TNORM_PRODUCT;
        config.enable_anfis = true;
        config.anfis_learning_rate = 0.01f;
        config.anfis_max_epochs = 100;
        config.anfis_convergence_tol = 1e-6f;

        fuzzy_inference_engine_t* engine = fuzzy_inference_create_custom(&config);
        if (!engine) return nullptr;

        // Single input: x in [0, 10]
        fuzzy_variable_t x_var;
        fuzzy_variable_init(&x_var, "x", 0.0f, 10.0f);
        fuzzy_variable_add_term(&x_var, "low", fuzzy_mf_gaussian(2.5f, 2.0f));
        fuzzy_variable_add_term(&x_var, "medium", fuzzy_mf_gaussian(5.0f, 2.0f));
        fuzzy_variable_add_term(&x_var, "high", fuzzy_mf_gaussian(7.5f, 2.0f));
        fuzzy_inference_add_input(engine, &x_var);

        // Output (Sugeno)
        fuzzy_variable_t y_var;
        fuzzy_variable_init(&y_var, "y", -50.0f, 50.0f);
        fuzzy_inference_add_output(engine, &y_var);

        // Rules with initial coefficients (will be trained)
        float coeffs1[] = {0.0f, 1.0f};  // y = c0 + c1*x
        fuzzy_rule_t rule1 = fuzzy_rule_sugeno(0, 0, 0, 0, coeffs1, 2, 1.0f);
        rule1.num_antecedents = 1;  // Single antecedent
        fuzzy_inference_add_rule(engine, &rule1);

        float coeffs2[] = {0.0f, 1.0f};
        fuzzy_rule_t rule2 = fuzzy_rule_sugeno(0, 1, 0, 0, coeffs2, 2, 1.0f);
        rule2.num_antecedents = 1;
        fuzzy_inference_add_rule(engine, &rule2);

        float coeffs3[] = {0.0f, 1.0f};
        fuzzy_rule_t rule3 = fuzzy_rule_sugeno(0, 2, 0, 0, coeffs3, 2, 1.0f);
        rule3.num_antecedents = 1;
        fuzzy_inference_add_rule(engine, &rule3);

        return engine;
    }

    // Create 2-input Sugeno for surface approximation
    fuzzy_inference_engine_t* createSugenoSurface() {
        fuzzy_inference_config_t config = fuzzy_inference_default_config();
        config.fis_type = FUZZY_FIS_SUGENO;
        config.and_method = FUZZY_TNORM_PRODUCT;
        config.enable_anfis = true;

        fuzzy_inference_engine_t* engine = fuzzy_inference_create_custom(&config);
        if (!engine) return nullptr;

        // Input 1: x1 in [0, 5]
        fuzzy_variable_t x1_var;
        fuzzy_variable_init(&x1_var, "x1", 0.0f, 5.0f);
        fuzzy_variable_add_term(&x1_var, "low", fuzzy_mf_gaussian(1.25f, 1.0f));
        fuzzy_variable_add_term(&x1_var, "high", fuzzy_mf_gaussian(3.75f, 1.0f));
        fuzzy_inference_add_input(engine, &x1_var);

        // Input 2: x2 in [0, 5]
        fuzzy_variable_t x2_var;
        fuzzy_variable_init(&x2_var, "x2", 0.0f, 5.0f);
        fuzzy_variable_add_term(&x2_var, "low", fuzzy_mf_gaussian(1.25f, 1.0f));
        fuzzy_variable_add_term(&x2_var, "high", fuzzy_mf_gaussian(3.75f, 1.0f));
        fuzzy_inference_add_input(engine, &x2_var);

        // Output
        fuzzy_variable_t y_var;
        fuzzy_variable_init(&y_var, "y", 0.0f, 50.0f);
        fuzzy_inference_add_output(engine, &y_var);

        // 4 rules for 2x2 grid
        float coeffs[] = {0.0f, 1.0f, 1.0f};  // y = c0 + c1*x1 + c2*x2
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                fuzzy_rule_t rule = fuzzy_rule_sugeno(0, i, 1, j, coeffs, 3, 1.0f);
                fuzzy_inference_add_rule(engine, &rule);
            }
        }

        return engine;
    }

    // Generate training data for y = sin(x)
    void generateSinData(std::vector<float>& inputs, std::vector<float>& targets, uint32_t n) {
        inputs.resize(n);
        targets.resize(n);
        for (uint32_t i = 0; i < n; i++) {
            inputs[i] = 10.0f * static_cast<float>(i) / (n - 1);  // 0 to 10
            targets[i] = 10.0f * std::sin(inputs[i] * 0.5f);  // Scale sin to reasonable range
        }
    }

    // Generate training data for y = x1 + x2
    void generateSumData(std::vector<float>& inputs, std::vector<float>& targets, uint32_t n) {
        inputs.resize(n * 2);
        targets.resize(n);
        for (uint32_t i = 0; i < n; i++) {
            float x1 = 5.0f * static_cast<float>(rand()) / RAND_MAX;
            float x2 = 5.0f * static_cast<float>(rand()) / RAND_MAX;
            inputs[i * 2 + 0] = x1;
            inputs[i * 2 + 1] = x2;
            targets[i] = x1 + x2;
        }
    }
};

//=============================================================================
// ANFIS Training Tests
//=============================================================================

TEST_F(FuzzyGPUANFISTest, BasicTraining) {
    cpu_engine = createSugenoApproximator();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    // Generate training data
    std::vector<float> inputs, targets;
    generateSinData(inputs, targets, 100);

    nimcp_gpu_anfis_params_t params = nimcp_gpu_anfis_params_default();
    params.max_epochs = 100;
    params.learning_rate = 0.01f;
    params.convergence_tolerance = 1e-4f;

    float final_error;
    bool ok = nimcp_gpu_anfis_train_raw(ctx, gpu_state, inputs.data(), targets.data(),
                                         inputs.size(), &final_error, &params);
    ASSERT_TRUE(ok) << "ANFIS training failed";

    EXPECT_LT(final_error, 10.0f) << "Training error should decrease significantly";

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

TEST_F(FuzzyGPUANFISTest, TrainingConvergence) {
    cpu_engine = createSugenoSurface();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    std::vector<float> inputs, targets;
    generateSumData(inputs, targets, 200);

    nimcp_gpu_anfis_params_t params = nimcp_gpu_anfis_params_default();
    params.max_epochs = 500;
    params.learning_rate = 0.005f;
    params.convergence_tolerance = 1e-5f;
    params.use_early_stopping = true;
    params.patience = 20;

    float final_error;
    bool ok = nimcp_gpu_anfis_train_raw(ctx, gpu_state, inputs.data(), targets.data(),
                                         targets.size(), &final_error, &params);
    ASSERT_TRUE(ok);

    // For simple linear function, error should be very small
    EXPECT_LT(final_error, 1.0f) << "Should approximate linear function well";

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

TEST_F(FuzzyGPUANFISTest, SingleEpochTraining) {
    cpu_engine = createSugenoApproximator();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    std::vector<float> inputs, targets;
    generateSinData(inputs, targets, 50);

    // Upload data to GPU
    float* d_inputs = nullptr;
    float* d_targets = nullptr;
    cudaMalloc(&d_inputs, inputs.size() * sizeof(float));
    cudaMalloc(&d_targets, targets.size() * sizeof(float));
    cudaMemcpy(d_inputs, inputs.data(), inputs.size() * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_targets, targets.data(), targets.size() * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_anfis_params_t params = nimcp_gpu_anfis_params_default();
    params.learning_rate = 0.01f;

    float epoch_error;
    bool ok = nimcp_gpu_anfis_train_epoch(ctx, gpu_state, d_inputs, d_targets,
                                           inputs.size(), &params, &epoch_error);
    ASSERT_TRUE(ok);
    EXPECT_GT(epoch_error, 0.0f) << "Epoch error should be computed";

    cudaFree(d_inputs);
    cudaFree(d_targets);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

TEST_F(FuzzyGPUANFISTest, ParameterDownloadToCPU) {
    cpu_engine = createSugenoApproximator();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    // Train on GPU
    std::vector<float> inputs, targets;
    generateSinData(inputs, targets, 100);

    nimcp_gpu_anfis_params_t params = nimcp_gpu_anfis_params_default();
    params.max_epochs = 50;

    float final_error;
    nimcp_gpu_anfis_train_raw(ctx, gpu_state, inputs.data(), targets.data(),
                               inputs.size(), &final_error, &params);

    // Download trained parameters back to CPU
    int err = nimcp_gpu_anfis_download_params(ctx, gpu_state, cpu_engine);
    ASSERT_EQ(err, 0) << "Parameter download failed";

    // Verify by running CPU inference
    fuzzy_inference_result_t result;
    float test_input = 5.0f;
    err = fuzzy_inference_evaluate(cpu_engine, &test_input, 1, &result);
    ASSERT_EQ(err, 0);

    // Should produce reasonable output (not NaN, not extreme)
    EXPECT_FALSE(std::isnan(result.crisp_outputs[0]));
    EXPECT_GT(result.crisp_outputs[0], -100.0f);
    EXPECT_LT(result.crisp_outputs[0], 100.0f);

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

TEST_F(FuzzyGPUANFISTest, MomentumTraining) {
    cpu_engine = createSugenoSurface();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    std::vector<float> inputs, targets;
    generateSumData(inputs, targets, 150);

    nimcp_gpu_anfis_params_t params = nimcp_gpu_anfis_params_default();
    params.max_epochs = 200;
    params.learning_rate = 0.01f;
    params.momentum = 0.9f;  // Use momentum
    params.use_momentum = true;

    float final_error;
    bool ok = nimcp_gpu_anfis_train_raw(ctx, gpu_state, inputs.data(), targets.data(),
                                         targets.size(), &final_error, &params);
    ASSERT_TRUE(ok);

    EXPECT_LT(final_error, 2.0f) << "Momentum should help convergence";

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

//=============================================================================
// Fuzzy Relation Composition Tests
//=============================================================================

TEST_F(FuzzyGPUANFISTest, RelationComposition) {
    // Test max-min composition of two fuzzy relations
    const uint32_t ROWS_A = 4;
    const uint32_t COLS_A = 3;  // Shared dimension
    const uint32_t COLS_B = 5;

    std::vector<float> rel_a(ROWS_A * COLS_A);
    std::vector<float> rel_b(COLS_A * COLS_B);

    // Initialize with random fuzzy values [0, 1]
    for (uint32_t i = 0; i < ROWS_A * COLS_A; i++) {
        rel_a[i] = static_cast<float>(rand()) / RAND_MAX;
    }
    for (uint32_t i = 0; i < COLS_A * COLS_B; i++) {
        rel_b[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    // Allocate device memory
    float* d_rel_a = nullptr;
    float* d_rel_b = nullptr;
    float* d_rel_out = nullptr;
    cudaMalloc(&d_rel_a, ROWS_A * COLS_A * sizeof(float));
    cudaMalloc(&d_rel_b, COLS_A * COLS_B * sizeof(float));
    cudaMalloc(&d_rel_out, ROWS_A * COLS_B * sizeof(float));

    cudaMemcpy(d_rel_a, rel_a.data(), ROWS_A * COLS_A * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_rel_b, rel_b.data(), COLS_A * COLS_B * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_relation_params_t params = {0};
    params.tnorm = FUZZY_TNORM_MIN;
    params.tconorm = FUZZY_TCONORM_MAX;

    bool ok = nimcp_gpu_fuzzy_relation_compose(ctx, d_rel_a, ROWS_A, COLS_A,
                                                 d_rel_b, COLS_B, d_rel_out, &params);
    ASSERT_TRUE(ok);

    std::vector<float> result(ROWS_A * COLS_B);
    cudaMemcpy(result.data(), d_rel_out, ROWS_A * COLS_B * sizeof(float), cudaMemcpyDeviceToHost);

    // Verify max-min composition manually for a few elements
    for (uint32_t i = 0; i < ROWS_A; i++) {
        for (uint32_t j = 0; j < COLS_B; j++) {
            float expected = 0.0f;
            for (uint32_t k = 0; k < COLS_A; k++) {
                float min_val = std::min(rel_a[i * COLS_A + k], rel_b[k * COLS_B + j]);
                expected = std::max(expected, min_val);
            }
            EXPECT_NEAR(result[i * COLS_B + j], expected, 1e-5f)
                << "Composition mismatch at (" << i << "," << j << ")";
        }
    }

    // All values should be in [0, 1]
    for (uint32_t i = 0; i < ROWS_A * COLS_B; i++) {
        EXPECT_GE(result[i], 0.0f);
        EXPECT_LE(result[i], 1.0f);
    }

    cudaFree(d_rel_a);
    cudaFree(d_rel_b);
    cudaFree(d_rel_out);
}

TEST_F(FuzzyGPUANFISTest, RelationCompositionProduct) {
    // Test max-product composition
    const uint32_t ROWS_A = 3;
    const uint32_t COLS_A = 3;
    const uint32_t COLS_B = 3;

    std::vector<float> rel_a = {
        0.2f, 0.8f, 0.5f,
        0.6f, 0.4f, 0.3f,
        0.9f, 0.1f, 0.7f
    };
    std::vector<float> rel_b = {
        0.5f, 0.3f, 0.8f,
        0.4f, 0.7f, 0.2f,
        0.6f, 0.5f, 0.4f
    };

    float* d_rel_a = nullptr;
    float* d_rel_b = nullptr;
    float* d_rel_out = nullptr;
    cudaMalloc(&d_rel_a, ROWS_A * COLS_A * sizeof(float));
    cudaMalloc(&d_rel_b, COLS_A * COLS_B * sizeof(float));
    cudaMalloc(&d_rel_out, ROWS_A * COLS_B * sizeof(float));

    cudaMemcpy(d_rel_a, rel_a.data(), ROWS_A * COLS_A * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_rel_b, rel_b.data(), COLS_A * COLS_B * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_relation_params_t params = {0};
    params.tnorm = FUZZY_TNORM_PRODUCT;
    params.tconorm = FUZZY_TCONORM_MAX;

    bool ok = nimcp_gpu_fuzzy_relation_compose(ctx, d_rel_a, ROWS_A, COLS_A,
                                                 d_rel_b, COLS_B, d_rel_out, &params);
    ASSERT_TRUE(ok);

    std::vector<float> result(ROWS_A * COLS_B);
    cudaMemcpy(result.data(), d_rel_out, ROWS_A * COLS_B * sizeof(float), cudaMemcpyDeviceToHost);

    // Verify max-product composition
    for (uint32_t i = 0; i < ROWS_A; i++) {
        for (uint32_t j = 0; j < COLS_B; j++) {
            float expected = 0.0f;
            for (uint32_t k = 0; k < COLS_A; k++) {
                float prod = rel_a[i * COLS_A + k] * rel_b[k * COLS_B + j];
                expected = std::max(expected, prod);
            }
            EXPECT_NEAR(result[i * COLS_B + j], expected, 1e-5f);
        }
    }

    cudaFree(d_rel_a);
    cudaFree(d_rel_b);
    cudaFree(d_rel_out);
}

TEST_F(FuzzyGPUANFISTest, BatchRelationComposition) {
    const uint32_t BATCH_SIZE = 10;
    const uint32_t ROWS_A = 4;
    const uint32_t COLS_A = 4;
    const uint32_t COLS_B = 4;

    std::vector<float> relations_a(BATCH_SIZE * ROWS_A * COLS_A);
    std::vector<float> relations_b(BATCH_SIZE * COLS_A * COLS_B);

    for (uint32_t b = 0; b < BATCH_SIZE; b++) {
        for (uint32_t i = 0; i < ROWS_A * COLS_A; i++) {
            relations_a[b * ROWS_A * COLS_A + i] = static_cast<float>(rand()) / RAND_MAX;
        }
        for (uint32_t i = 0; i < COLS_A * COLS_B; i++) {
            relations_b[b * COLS_A * COLS_B + i] = static_cast<float>(rand()) / RAND_MAX;
        }
    }

    float* d_rel_a = nullptr;
    float* d_rel_b = nullptr;
    float* d_rel_out = nullptr;
    cudaMalloc(&d_rel_a, relations_a.size() * sizeof(float));
    cudaMalloc(&d_rel_b, relations_b.size() * sizeof(float));
    cudaMalloc(&d_rel_out, BATCH_SIZE * ROWS_A * COLS_B * sizeof(float));

    cudaMemcpy(d_rel_a, relations_a.data(), relations_a.size() * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_rel_b, relations_b.data(), relations_b.size() * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_relation_params_t params = {0};
    params.tnorm = FUZZY_TNORM_MIN;
    params.tconorm = FUZZY_TCONORM_MAX;

    bool ok = nimcp_gpu_fuzzy_relation_compose_batch(ctx, d_rel_a, d_rel_b, BATCH_SIZE,
                                                       ROWS_A, COLS_A, COLS_B, d_rel_out, &params);
    ASSERT_TRUE(ok);

    std::vector<float> results(BATCH_SIZE * ROWS_A * COLS_B);
    cudaMemcpy(results.data(), d_rel_out, results.size() * sizeof(float), cudaMemcpyDeviceToHost);

    // All values should be in [0, 1]
    for (size_t i = 0; i < results.size(); i++) {
        EXPECT_GE(results[i], 0.0f);
        EXPECT_LE(results[i], 1.0f);
    }

    cudaFree(d_rel_a);
    cudaFree(d_rel_b);
    cudaFree(d_rel_out);
}

//=============================================================================
// Type Conversion Tests
//=============================================================================

TEST_F(FuzzyGPUANFISTest, MFTypeConversion) {
    fuzzy_mf_t cpu_mf = fuzzy_mf_triangular(10.0f, 25.0f, 40.0f);

    fuzzy_gpu_mf_t gpu_mf = nimcp_gpu_fuzzy_mf_from_cpu(&cpu_mf, FUZZY_HEDGE_NONE, 0.0f);

    EXPECT_EQ(gpu_mf.type, static_cast<uint32_t>(FUZZY_MF_TRIANGULAR));
    EXPECT_EQ(gpu_mf.hedge, static_cast<uint32_t>(FUZZY_HEDGE_NONE));
    EXPECT_NEAR(gpu_mf.params[0], 10.0f, 1e-6f);
    EXPECT_NEAR(gpu_mf.params[1], 25.0f, 1e-6f);
    EXPECT_NEAR(gpu_mf.params[2], 40.0f, 1e-6f);
    EXPECT_EQ(gpu_mf.num_params, 3u);
}

TEST_F(FuzzyGPUANFISTest, MFTypeConversionWithHedge) {
    fuzzy_mf_t cpu_mf = fuzzy_mf_gaussian(50.0f, 10.0f);

    fuzzy_gpu_mf_t gpu_mf = nimcp_gpu_fuzzy_mf_from_cpu(&cpu_mf, FUZZY_HEDGE_VERY, 0.1f);

    EXPECT_EQ(gpu_mf.type, static_cast<uint32_t>(FUZZY_MF_GAUSSIAN));
    EXPECT_EQ(gpu_mf.hedge, static_cast<uint32_t>(FUZZY_HEDGE_VERY));
    EXPECT_NEAR(gpu_mf.alpha_cut, 0.1f, 1e-6f);
}

TEST_F(FuzzyGPUANFISTest, VariableTypeConversion) {
    fuzzy_variable_t cpu_var;
    fuzzy_variable_init(&cpu_var, "test", 0.0f, 100.0f);
    fuzzy_variable_add_term(&cpu_var, "low", fuzzy_mf_triangular(0.0f, 0.0f, 50.0f));
    fuzzy_variable_add_term(&cpu_var, "high", fuzzy_mf_triangular(50.0f, 100.0f, 100.0f));

    fuzzy_gpu_variable_t gpu_var;
    int err = nimcp_gpu_fuzzy_variable_from_cpu(&cpu_var, &gpu_var);
    ASSERT_EQ(err, 0);

    EXPECT_NEAR(gpu_var.universe_min, 0.0f, 1e-6f);
    EXPECT_NEAR(gpu_var.universe_max, 100.0f, 1e-6f);
    EXPECT_EQ(gpu_var.num_terms, 2u);
}

TEST_F(FuzzyGPUANFISTest, RuleTypeConversion) {
    fuzzy_rule_t cpu_rule = fuzzy_rule_mamdani(0, 1, 1, 2, 0, 0, 0.8f);

    fuzzy_gpu_rule_t gpu_rule;
    int err = nimcp_gpu_fuzzy_rule_from_cpu(&cpu_rule, &gpu_rule);
    ASSERT_EQ(err, 0);

    EXPECT_EQ(gpu_rule.num_antecedents, 2u);
    EXPECT_NEAR(gpu_rule.weight, 0.8f, 1e-6f);
    EXPECT_EQ(gpu_rule.antecedents[0].var_index, 0u);
    EXPECT_EQ(gpu_rule.antecedents[0].term_index, 1u);
    EXPECT_EQ(gpu_rule.antecedents[1].var_index, 1u);
    EXPECT_EQ(gpu_rule.antecedents[1].term_index, 2u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(FuzzyGPUANFISTest, ANFISStatistics) {
    nimcp_gpu_fuzzy_reset_stats();

    cpu_engine = createSugenoApproximator();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    std::vector<float> inputs, targets;
    generateSinData(inputs, targets, 50);

    nimcp_gpu_anfis_params_t params = nimcp_gpu_anfis_params_default();
    params.max_epochs = 10;

    float final_error;
    nimcp_gpu_anfis_train_raw(ctx, gpu_state, inputs.data(), targets.data(),
                               inputs.size(), &final_error, &params);

    nimcp_gpu_fuzzy_stats_t stats;
    int err = nimcp_gpu_fuzzy_get_stats(&stats);
    ASSERT_EQ(err, 0);

    EXPECT_EQ(stats.anfis_epochs, 10u) << "Should record 10 epochs";
    EXPECT_GT(stats.total_kernel_time_ms, 0.0f);

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

//=============================================================================
// Large Scale Tests
//=============================================================================

TEST_F(FuzzyGPUANFISTest, LargeRelationComposition) {
    const uint32_t SIZE = 128;

    std::vector<float> rel_a(SIZE * SIZE);
    std::vector<float> rel_b(SIZE * SIZE);

    for (uint32_t i = 0; i < SIZE * SIZE; i++) {
        rel_a[i] = static_cast<float>(rand()) / RAND_MAX;
        rel_b[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    float* d_rel_a = nullptr;
    float* d_rel_b = nullptr;
    float* d_rel_out = nullptr;
    cudaMalloc(&d_rel_a, SIZE * SIZE * sizeof(float));
    cudaMalloc(&d_rel_b, SIZE * SIZE * sizeof(float));
    cudaMalloc(&d_rel_out, SIZE * SIZE * sizeof(float));

    cudaMemcpy(d_rel_a, rel_a.data(), SIZE * SIZE * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_rel_b, rel_b.data(), SIZE * SIZE * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_gpu_relation_params_t params = {0};
    params.tnorm = FUZZY_TNORM_MIN;
    params.tconorm = FUZZY_TCONORM_MAX;

    bool ok = nimcp_gpu_fuzzy_relation_compose(ctx, d_rel_a, SIZE, SIZE,
                                                 d_rel_b, SIZE, d_rel_out, &params);
    ASSERT_TRUE(ok) << "Large relation composition failed";

    std::vector<float> result(SIZE * SIZE);
    cudaMemcpy(result.data(), d_rel_out, SIZE * SIZE * sizeof(float), cudaMemcpyDeviceToHost);

    // Spot check a few values
    for (int test = 0; test < 10; test++) {
        uint32_t i = rand() % SIZE;
        uint32_t j = rand() % SIZE;
        float expected = 0.0f;
        for (uint32_t k = 0; k < SIZE; k++) {
            float min_val = std::min(rel_a[i * SIZE + k], rel_b[k * SIZE + j]);
            expected = std::max(expected, min_val);
        }
        EXPECT_NEAR(result[i * SIZE + j], expected, 1e-4f);
    }

    cudaFree(d_rel_a);
    cudaFree(d_rel_b);
    cudaFree(d_rel_out);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
