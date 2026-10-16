//=============================================================================
// test_fuzzy_gpu_inference.cpp - Integration Tests for GPU Fuzzy Inference
//=============================================================================
/**
 * @file test_fuzzy_gpu_inference.cpp
 * @brief Integration tests for GPU-accelerated fuzzy inference
 *
 * Tests complete inference pipelines comparing GPU vs CPU results,
 * batch processing, state management, and synchronization.
 *
 * COVERAGE:
 *   - GPU state creation from CPU engine
 *   - Batch inference with Mamdani FIS
 *   - Batch inference with Sugeno FIS
 *   - GPU vs CPU result equivalence
 *   - Large batch processing
 *   - State synchronization
 *   - Memory management
 *   - Error handling
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

class FuzzyGPUInferenceTest : public ::testing::Test {
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

    // Create a simple Mamdani FIS for temperature control
    fuzzy_inference_engine_t* createTemperatureControlFIS() {
        fuzzy_inference_config_t config = fuzzy_inference_default_config();
        config.fis_type = FUZZY_FIS_MAMDANI;
        config.defuzz_method = FUZZY_DEFUZZ_CENTROID;
        config.and_method = FUZZY_TNORM_MIN;
        config.or_method = FUZZY_TCONORM_MAX;

        fuzzy_inference_engine_t* engine = fuzzy_inference_create_custom(&config);
        if (!engine) return nullptr;

        // Input 1: Temperature (0-100)
        fuzzy_variable_t temp_var;
        fuzzy_variable_init(&temp_var, "temperature", 0.0f, 100.0f);
        fuzzy_variable_add_term(&temp_var, "cold", fuzzy_mf_triangular(0.0f, 0.0f, 40.0f));
        fuzzy_variable_add_term(&temp_var, "warm", fuzzy_mf_triangular(20.0f, 50.0f, 80.0f));
        fuzzy_variable_add_term(&temp_var, "hot", fuzzy_mf_triangular(60.0f, 100.0f, 100.0f));
        fuzzy_inference_add_input(engine, &temp_var);

        // Input 2: Humidity (0-100)
        fuzzy_variable_t humid_var;
        fuzzy_variable_init(&humid_var, "humidity", 0.0f, 100.0f);
        fuzzy_variable_add_term(&humid_var, "dry", fuzzy_mf_triangular(0.0f, 0.0f, 50.0f));
        fuzzy_variable_add_term(&humid_var, "normal", fuzzy_mf_triangular(25.0f, 50.0f, 75.0f));
        fuzzy_variable_add_term(&humid_var, "humid", fuzzy_mf_triangular(50.0f, 100.0f, 100.0f));
        fuzzy_inference_add_input(engine, &humid_var);

        // Output: Fan speed (0-100)
        fuzzy_variable_t fan_var;
        fuzzy_variable_init(&fan_var, "fan_speed", 0.0f, 100.0f);
        fuzzy_variable_add_term(&fan_var, "slow", fuzzy_mf_triangular(0.0f, 0.0f, 50.0f));
        fuzzy_variable_add_term(&fan_var, "medium", fuzzy_mf_triangular(25.0f, 50.0f, 75.0f));
        fuzzy_variable_add_term(&fan_var, "fast", fuzzy_mf_triangular(50.0f, 100.0f, 100.0f));
        fuzzy_inference_add_output(engine, &fan_var);

        // Rules
        // IF cold AND dry THEN slow
        fuzzy_rule_t rule1 = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
        fuzzy_inference_add_rule(engine, &rule1);

        // IF warm AND normal THEN medium
        fuzzy_rule_t rule2 = fuzzy_rule_mamdani(0, 1, 1, 1, 0, 1, 1.0f);
        fuzzy_inference_add_rule(engine, &rule2);

        // IF hot AND humid THEN fast
        fuzzy_rule_t rule3 = fuzzy_rule_mamdani(0, 2, 1, 2, 0, 2, 1.0f);
        fuzzy_inference_add_rule(engine, &rule3);

        // IF hot AND normal THEN fast
        fuzzy_rule_t rule4 = fuzzy_rule_mamdani(0, 2, 1, 1, 0, 2, 1.0f);
        fuzzy_inference_add_rule(engine, &rule4);

        // IF warm AND humid THEN medium
        fuzzy_rule_t rule5 = fuzzy_rule_mamdani(0, 1, 1, 2, 0, 1, 1.0f);
        fuzzy_inference_add_rule(engine, &rule5);

        return engine;
    }

    // Create a Sugeno FIS for market analysis
    fuzzy_inference_engine_t* createSugenoMarketFIS() {
        fuzzy_inference_config_t config = fuzzy_inference_default_config();
        config.fis_type = FUZZY_FIS_SUGENO;
        config.and_method = FUZZY_TNORM_PRODUCT;

        fuzzy_inference_engine_t* engine = fuzzy_inference_create_custom(&config);
        if (!engine) return nullptr;

        // Input 1: Price (0-200)
        fuzzy_variable_t price_var;
        fuzzy_variable_init(&price_var, "price", 0.0f, 200.0f);
        fuzzy_variable_add_term(&price_var, "low", fuzzy_mf_gaussian(50.0f, 25.0f));
        fuzzy_variable_add_term(&price_var, "medium", fuzzy_mf_gaussian(100.0f, 25.0f));
        fuzzy_variable_add_term(&price_var, "high", fuzzy_mf_gaussian(150.0f, 25.0f));
        fuzzy_inference_add_input(engine, &price_var);

        // Input 2: Volume (0-1000)
        fuzzy_variable_t vol_var;
        fuzzy_variable_init(&vol_var, "volume", 0.0f, 1000.0f);
        fuzzy_variable_add_term(&vol_var, "low", fuzzy_mf_gaussian(200.0f, 100.0f));
        fuzzy_variable_add_term(&vol_var, "high", fuzzy_mf_gaussian(800.0f, 100.0f));
        fuzzy_inference_add_input(engine, &vol_var);

        // Output: (Sugeno uses polynomial consequents)
        fuzzy_variable_t signal_var;
        fuzzy_variable_init(&signal_var, "signal", -1.0f, 1.0f);
        fuzzy_inference_add_output(engine, &signal_var);

        // Rules with Sugeno consequents: output = c0 + c1*price + c2*volume
        // IF price low AND volume high THEN buy (positive signal)
        float coeffs1[] = {0.5f, -0.002f, 0.0005f};  // Positive when low price, high vol
        fuzzy_rule_t rule1 = fuzzy_rule_sugeno(0, 0, 1, 1, coeffs1, 3, 1.0f);
        fuzzy_inference_add_rule(engine, &rule1);

        // IF price high AND volume low THEN sell (negative signal)
        float coeffs2[] = {-0.5f, 0.002f, -0.0005f};
        fuzzy_rule_t rule2 = fuzzy_rule_sugeno(0, 2, 1, 0, coeffs2, 3, 1.0f);
        fuzzy_inference_add_rule(engine, &rule2);

        // IF price medium THEN hold (neutral)
        float coeffs3[] = {0.0f, 0.0f, 0.0f};
        fuzzy_rule_t rule3 = fuzzy_rule_sugeno(0, 1, 1, 0, coeffs3, 3, 0.5f);
        fuzzy_inference_add_rule(engine, &rule3);

        return engine;
    }
};

//=============================================================================
// State Lifecycle Tests
//=============================================================================

TEST_F(FuzzyGPUInferenceTest, StateCreationMamdani) {
    cpu_engine = createTemperatureControlFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr) << "GPU state creation failed";
    EXPECT_TRUE(nimcp_gpu_fuzzy_state_is_valid(gpu_state));

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

TEST_F(FuzzyGPUInferenceTest, StateCreationSugeno) {
    cpu_engine = createSugenoMarketFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr) << "GPU state creation for Sugeno failed";
    EXPECT_TRUE(nimcp_gpu_fuzzy_state_is_valid(gpu_state));

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

TEST_F(FuzzyGPUInferenceTest, StateCreationWithCapacity) {
    cpu_engine = createTemperatureControlFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state =
        nimcp_gpu_fuzzy_state_create_with_capacity(ctx, cpu_engine, 10000);
    ASSERT_NE(gpu_state, nullptr);
    EXPECT_TRUE(nimcp_gpu_fuzzy_state_is_valid(gpu_state));

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

TEST_F(FuzzyGPUInferenceTest, StateNullSafety) {
    // Destroy with NULL should be safe
    nimcp_gpu_fuzzy_state_destroy(nullptr);

    // Validation with NULL should return false
    EXPECT_FALSE(nimcp_gpu_fuzzy_state_is_valid(nullptr));
}

//=============================================================================
// GPU vs CPU Equivalence Tests
//=============================================================================

TEST_F(FuzzyGPUInferenceTest, MamdaniGPUvsCPUEquivalence) {
    cpu_engine = createTemperatureControlFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    const uint32_t BATCH_SIZE = 100;
    const uint32_t NUM_INPUTS = 2;
    const uint32_t NUM_OUTPUTS = 1;

    // Generate test inputs
    std::vector<float> inputs(BATCH_SIZE * NUM_INPUTS);
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        inputs[i * NUM_INPUTS + 0] = 10.0f + 80.0f * (static_cast<float>(i) / BATCH_SIZE);  // temp
        inputs[i * NUM_INPUTS + 1] = 20.0f + 60.0f * (static_cast<float>(rand()) / RAND_MAX);  // humidity
    }

    // CPU inference
    std::vector<fuzzy_inference_result_t> cpu_results(BATCH_SIZE);
    int err = fuzzy_inference_evaluate_batch(cpu_engine, inputs.data(), BATCH_SIZE, NUM_INPUTS, cpu_results.data());
    ASSERT_EQ(err, 0);

    // GPU inference
    uint32_t in_dims[] = {BATCH_SIZE, NUM_INPUTS};
    uint32_t out_dims[] = {BATCH_SIZE, NUM_OUTPUTS};
    nimcp_gpu_fuzzy_tensor_t gpu_inputs = nimcp_gpu_fuzzy_tensor_create(ctx, in_dims, 2);
    nimcp_gpu_fuzzy_tensor_t gpu_outputs = nimcp_gpu_fuzzy_tensor_create(ctx, out_dims, 2);

    ASSERT_TRUE(nimcp_gpu_fuzzy_tensor_upload(ctx, &gpu_inputs, inputs.data()));

    nimcp_gpu_inference_params_t params = nimcp_gpu_inference_params_default();
    params.batch_size = BATCH_SIZE;

    bool ok = nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, &gpu_inputs, &gpu_outputs, &params);
    ASSERT_TRUE(ok);

    std::vector<float> gpu_results_data(BATCH_SIZE * NUM_OUTPUTS);
    ASSERT_TRUE(nimcp_gpu_fuzzy_tensor_download(ctx, &gpu_outputs, gpu_results_data.data()));

    // Compare results
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        EXPECT_NEAR(gpu_results_data[i], cpu_results[i].crisp_outputs[0], 1.0f)
            << "Mismatch at sample " << i
            << " (temp=" << inputs[i*2] << ", humid=" << inputs[i*2+1] << ")";
    }

    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_inputs);
    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_outputs);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

TEST_F(FuzzyGPUInferenceTest, SugenoGPUvsCPUEquivalence) {
    cpu_engine = createSugenoMarketFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    const uint32_t BATCH_SIZE = 50;
    const uint32_t NUM_INPUTS = 2;
    const uint32_t NUM_OUTPUTS = 1;

    std::vector<float> inputs(BATCH_SIZE * NUM_INPUTS);
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        inputs[i * NUM_INPUTS + 0] = 20.0f + 160.0f * (static_cast<float>(i) / BATCH_SIZE);  // price
        inputs[i * NUM_INPUTS + 1] = 100.0f + 800.0f * (static_cast<float>(rand()) / RAND_MAX);  // volume
    }

    // CPU inference
    std::vector<fuzzy_inference_result_t> cpu_results(BATCH_SIZE);
    int err = fuzzy_inference_evaluate_batch(cpu_engine, inputs.data(), BATCH_SIZE, NUM_INPUTS, cpu_results.data());
    ASSERT_EQ(err, 0);

    // GPU inference
    uint32_t in_dims[] = {BATCH_SIZE, NUM_INPUTS};
    uint32_t out_dims[] = {BATCH_SIZE, NUM_OUTPUTS};
    nimcp_gpu_fuzzy_tensor_t gpu_inputs = nimcp_gpu_fuzzy_tensor_create(ctx, in_dims, 2);
    nimcp_gpu_fuzzy_tensor_t gpu_outputs = nimcp_gpu_fuzzy_tensor_create(ctx, out_dims, 2);

    nimcp_gpu_fuzzy_tensor_upload(ctx, &gpu_inputs, inputs.data());

    nimcp_gpu_inference_params_t params = nimcp_gpu_inference_params_default();
    params.batch_size = BATCH_SIZE;

    bool ok = nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, &gpu_inputs, &gpu_outputs, &params);
    ASSERT_TRUE(ok);

    std::vector<float> gpu_results_data(BATCH_SIZE * NUM_OUTPUTS);
    nimcp_gpu_fuzzy_tensor_download(ctx, &gpu_outputs, gpu_results_data.data());

    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        EXPECT_NEAR(gpu_results_data[i], cpu_results[i].crisp_outputs[0], 0.1f)
            << "Sugeno mismatch at sample " << i;
    }

    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_inputs);
    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_outputs);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

//=============================================================================
// Large Batch Processing Tests
//=============================================================================

TEST_F(FuzzyGPUInferenceTest, LargeBatchInference) {
    cpu_engine = createTemperatureControlFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state =
        nimcp_gpu_fuzzy_state_create_with_capacity(ctx, cpu_engine, 50000);
    ASSERT_NE(gpu_state, nullptr);

    const uint32_t BATCH_SIZE = 10000;
    const uint32_t NUM_INPUTS = 2;
    const uint32_t NUM_OUTPUTS = 1;

    std::vector<float> inputs(BATCH_SIZE * NUM_INPUTS);
    for (uint32_t i = 0; i < BATCH_SIZE * NUM_INPUTS; i++) {
        inputs[i] = 100.0f * (static_cast<float>(rand()) / RAND_MAX);
    }

    uint32_t in_dims[] = {BATCH_SIZE, NUM_INPUTS};
    uint32_t out_dims[] = {BATCH_SIZE, NUM_OUTPUTS};
    nimcp_gpu_fuzzy_tensor_t gpu_inputs = nimcp_gpu_fuzzy_tensor_create(ctx, in_dims, 2);
    nimcp_gpu_fuzzy_tensor_t gpu_outputs = nimcp_gpu_fuzzy_tensor_create(ctx, out_dims, 2);

    ASSERT_TRUE(nimcp_gpu_fuzzy_tensor_upload(ctx, &gpu_inputs, inputs.data()));

    nimcp_gpu_inference_params_t params = nimcp_gpu_inference_params_default();
    params.batch_size = BATCH_SIZE;

    bool ok = nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, &gpu_inputs, &gpu_outputs, &params);
    ASSERT_TRUE(ok) << "Large batch inference failed";

    std::vector<float> results(BATCH_SIZE);
    ASSERT_TRUE(nimcp_gpu_fuzzy_tensor_download(ctx, &gpu_outputs, results.data()));

    // Verify all outputs are in valid range
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        EXPECT_GE(results[i], 0.0f) << "Output below universe min at " << i;
        EXPECT_LE(results[i], 100.0f) << "Output above universe max at " << i;
    }

    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_inputs);
    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_outputs);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

TEST_F(FuzzyGPUInferenceTest, RecommendedBatchSize) {
    cpu_engine = createTemperatureControlFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    uint32_t recommended = nimcp_gpu_fuzzy_recommended_batch_size(ctx, gpu_state);
    EXPECT_GT(recommended, 0u) << "Recommended batch size should be positive";
    EXPECT_LE(recommended, 1000000u) << "Recommended batch size should be reasonable";

    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

//=============================================================================
// Inference with Rule Strengths
//=============================================================================

TEST_F(FuzzyGPUInferenceTest, InferenceWithRuleStrengths) {
    cpu_engine = createTemperatureControlFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    const uint32_t BATCH_SIZE = 10;
    const uint32_t NUM_INPUTS = 2;
    const uint32_t NUM_OUTPUTS = 1;
    const uint32_t NUM_RULES = fuzzy_inference_get_rule_count(cpu_engine);

    std::vector<float> inputs(BATCH_SIZE * NUM_INPUTS);
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        inputs[i * NUM_INPUTS + 0] = 50.0f;  // Middle temperature
        inputs[i * NUM_INPUTS + 1] = 50.0f;  // Middle humidity
    }

    uint32_t in_dims[] = {BATCH_SIZE, NUM_INPUTS};
    uint32_t out_dims[] = {BATCH_SIZE, NUM_OUTPUTS};
    uint32_t strength_dims[] = {BATCH_SIZE, static_cast<uint32_t>(NUM_RULES)};

    nimcp_gpu_fuzzy_tensor_t gpu_inputs = nimcp_gpu_fuzzy_tensor_create(ctx, in_dims, 2);
    nimcp_gpu_fuzzy_tensor_t gpu_outputs = nimcp_gpu_fuzzy_tensor_create(ctx, out_dims, 2);
    nimcp_gpu_fuzzy_tensor_t gpu_strengths = nimcp_gpu_fuzzy_tensor_create(ctx, strength_dims, 2);

    nimcp_gpu_fuzzy_tensor_upload(ctx, &gpu_inputs, inputs.data());

    nimcp_gpu_inference_params_t params = nimcp_gpu_inference_params_default();
    params.batch_size = BATCH_SIZE;

    bool ok = nimcp_gpu_fuzzy_inference_batch_with_strengths(ctx, gpu_state, &gpu_inputs,
                                                              &gpu_outputs, &gpu_strengths, &params);
    ASSERT_TRUE(ok);

    std::vector<float> strengths(BATCH_SIZE * NUM_RULES);
    nimcp_gpu_fuzzy_tensor_download(ctx, &gpu_strengths, strengths.data());

    // All rule strengths should be in [0, 1]
    for (uint32_t i = 0; i < BATCH_SIZE * NUM_RULES; i++) {
        EXPECT_GE(strengths[i], 0.0f) << "Rule strength below 0 at " << i;
        EXPECT_LE(strengths[i], 1.0f) << "Rule strength above 1 at " << i;
    }

    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_inputs);
    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_outputs);
    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_strengths);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

//=============================================================================
// State Synchronization Tests
//=============================================================================

TEST_F(FuzzyGPUInferenceTest, StateSyncAfterRuleChange) {
    cpu_engine = createTemperatureControlFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    // Run initial inference
    const uint32_t BATCH_SIZE = 10;
    float inputs[] = {75.0f, 75.0f};  // Hot and humid

    uint32_t in_dims[] = {1, 2};
    uint32_t out_dims[] = {1, 1};
    nimcp_gpu_fuzzy_tensor_t gpu_inputs = nimcp_gpu_fuzzy_tensor_create(ctx, in_dims, 2);
    nimcp_gpu_fuzzy_tensor_t gpu_outputs = nimcp_gpu_fuzzy_tensor_create(ctx, out_dims, 2);

    nimcp_gpu_fuzzy_tensor_upload(ctx, &gpu_inputs, inputs);

    nimcp_gpu_inference_params_t params = nimcp_gpu_inference_params_default();
    params.batch_size = 1;

    nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, &gpu_inputs, &gpu_outputs, &params);

    float result1;
    nimcp_gpu_fuzzy_tensor_download(ctx, &gpu_outputs, &result1);

    // Add a new rule to CPU engine
    fuzzy_rule_t new_rule = fuzzy_rule_mamdani(0, 2, 1, 2, 0, 0, 1.0f);  // Hot+humid -> slow (unusual)
    fuzzy_inference_add_rule(cpu_engine, &new_rule);

    // Sync GPU state
    int err = nimcp_gpu_fuzzy_state_sync(ctx, gpu_state, cpu_engine);
    ASSERT_EQ(err, 0);

    // Run inference again
    nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, &gpu_inputs, &gpu_outputs, &params);

    float result2;
    nimcp_gpu_fuzzy_tensor_download(ctx, &gpu_outputs, &result2);

    // Results should be different after adding conflicting rule
    EXPECT_NE(result1, result2) << "Results should change after rule update";

    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_inputs);
    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_outputs);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

//=============================================================================
// Tensor Operations Tests
//=============================================================================

TEST_F(FuzzyGPUInferenceTest, TensorUploadDownload) {
    const uint32_t N = 1000;
    uint32_t dims[] = {N, 4};

    nimcp_gpu_fuzzy_tensor_t tensor = nimcp_gpu_fuzzy_tensor_create(ctx, dims, 2);
    ASSERT_NE(tensor.d_data, nullptr);
    ASSERT_EQ(tensor.total_elements, N * 4);

    std::vector<float> data(N * 4);
    for (uint32_t i = 0; i < N * 4; i++) {
        data[i] = static_cast<float>(i);
    }

    ASSERT_TRUE(nimcp_gpu_fuzzy_tensor_upload(ctx, &tensor, data.data()));

    std::vector<float> downloaded(N * 4);
    ASSERT_TRUE(nimcp_gpu_fuzzy_tensor_download(ctx, &tensor, downloaded.data()));

    for (uint32_t i = 0; i < N * 4; i++) {
        EXPECT_EQ(data[i], downloaded[i]) << "Mismatch at index " << i;
    }

    nimcp_gpu_fuzzy_tensor_destroy(ctx, &tensor);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(FuzzyGPUInferenceTest, StatisticsCollection) {
    nimcp_gpu_fuzzy_reset_stats();

    cpu_engine = createTemperatureControlFIS();
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    const uint32_t BATCH_SIZE = 100;
    std::vector<float> inputs(BATCH_SIZE * 2);
    for (uint32_t i = 0; i < BATCH_SIZE * 2; i++) {
        inputs[i] = 50.0f;
    }

    uint32_t in_dims[] = {BATCH_SIZE, 2};
    uint32_t out_dims[] = {BATCH_SIZE, 1};
    nimcp_gpu_fuzzy_tensor_t gpu_inputs = nimcp_gpu_fuzzy_tensor_create(ctx, in_dims, 2);
    nimcp_gpu_fuzzy_tensor_t gpu_outputs = nimcp_gpu_fuzzy_tensor_create(ctx, out_dims, 2);
    nimcp_gpu_fuzzy_tensor_upload(ctx, &gpu_inputs, inputs.data());

    nimcp_gpu_inference_params_t params = nimcp_gpu_inference_params_default();
    params.batch_size = BATCH_SIZE;

    // Run multiple inferences
    for (int i = 0; i < 5; i++) {
        nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, &gpu_inputs, &gpu_outputs, &params);
    }

    nimcp_gpu_fuzzy_stats_t stats;
    int err = nimcp_gpu_fuzzy_get_stats(&stats);
    ASSERT_EQ(err, 0);

    EXPECT_EQ(stats.batch_inferences, 5u) << "Should have 5 batch inferences";
    EXPECT_EQ(stats.samples_processed, 5u * BATCH_SIZE) << "Should process 500 samples";
    EXPECT_GT(stats.total_kernel_time_ms, 0.0f) << "Kernel time should be recorded";

    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_inputs);
    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_outputs);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(FuzzyGPUInferenceTest, InvalidContextHandling) {
    cpu_engine = createTemperatureControlFIS();
    ASSERT_NE(cpu_engine, nullptr);

    // NULL context should fail gracefully
    nimcp_gpu_fuzzy_inference_state_t* state = nimcp_gpu_fuzzy_state_create(nullptr, cpu_engine);
    EXPECT_EQ(state, nullptr);
}

TEST_F(FuzzyGPUInferenceTest, InvalidEngineHandling) {
    // NULL engine should fail gracefully
    nimcp_gpu_fuzzy_inference_state_t* state = nimcp_gpu_fuzzy_state_create(ctx, nullptr);
    EXPECT_EQ(state, nullptr);
}

TEST_F(FuzzyGPUInferenceTest, DimensionMismatch) {
    cpu_engine = createTemperatureControlFIS();  // 2 inputs
    ASSERT_NE(cpu_engine, nullptr);

    nimcp_gpu_fuzzy_inference_state_t* gpu_state = nimcp_gpu_fuzzy_state_create(ctx, cpu_engine);
    ASSERT_NE(gpu_state, nullptr);

    // Create tensor with wrong input dimension (3 instead of 2)
    uint32_t wrong_dims[] = {10, 3};  // Wrong: 3 inputs instead of 2
    uint32_t out_dims[] = {10, 1};
    nimcp_gpu_fuzzy_tensor_t gpu_inputs = nimcp_gpu_fuzzy_tensor_create(ctx, wrong_dims, 2);
    nimcp_gpu_fuzzy_tensor_t gpu_outputs = nimcp_gpu_fuzzy_tensor_create(ctx, out_dims, 2);

    nimcp_gpu_inference_params_t params = nimcp_gpu_inference_params_default();
    params.batch_size = 10;

    // Should detect dimension mismatch
    bool ok = nimcp_gpu_fuzzy_inference_batch(ctx, gpu_state, &gpu_inputs, &gpu_outputs, &params);
    EXPECT_FALSE(ok) << "Should fail on dimension mismatch";

    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_inputs);
    nimcp_gpu_fuzzy_tensor_destroy(ctx, &gpu_outputs);
    nimcp_gpu_fuzzy_state_destroy(gpu_state);
}

//=============================================================================
// Availability Check
//=============================================================================

TEST_F(FuzzyGPUInferenceTest, AvailabilityCheck) {
    bool available = nimcp_gpu_fuzzy_is_available();
    EXPECT_TRUE(available) << "GPU fuzzy should be available (we have a context)";
}

TEST_F(FuzzyGPUInferenceTest, ErrorMessageRetrieval) {
    // After successful operations, error should be empty or null
    const char* error = nimcp_gpu_fuzzy_get_last_error();
    // Just verify it doesn't crash - error may or may not be set
    (void)error;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
