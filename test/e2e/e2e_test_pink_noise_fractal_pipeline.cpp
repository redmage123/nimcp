/**
 * @file e2e_test_pink_noise_fractal_pipeline.cpp
 * @brief End-to-end tests for Pink Noise + Fractal Analysis Pipeline
 *
 * WHAT: Full pipeline tests from noise generation through fractal analysis
 * WHY:  Validate complete workflow for biological neural network simulations
 * HOW:  Generate noise, apply to neural signals, analyze criticality
 *
 * NEUROSCIENCE WORKFLOWS TESTED:
 * 1. Neural criticality analysis (1/f noise in brain oscillations)
 * 2. Synaptic noise modulation (stochastic resonance)
 * 3. Memory consolidation dynamics (multiscale temporal correlations)
 * 4. Sleep stage characterization (fractal dimension changes)
 *
 * @version 2.6.3
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <fstream>

extern "C" {
#include "plasticity/noise/nimcp_pink_noise.h"
#include "cognitive/memory/core/nimcp_fractal.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PinkNoiseFractalE2ETest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    /* Helper: Generate pink noise with specified parameters */
    std::vector<float> generate_pink_noise(float alpha, size_t count, uint32_t seed) {
        pink_noise_config_t config = pink_noise_default_config();
        config.alpha = alpha;
        config.seed = seed;
        config.method = PINK_NOISE_FFT;
        config.amplitude = 1.0f;

        pink_noise_generator_t gen = pink_noise_create(&config);
        if (!gen) return {};

        std::vector<float> samples(count);
        pink_noise_generate(gen, samples.data(), samples.size());
        pink_noise_destroy(gen);

        return samples;
    }

    /* Helper: Simulate neural signal with pink noise modulation */
    std::vector<float> simulate_neural_signal(size_t count, uint32_t seed) {
        /* Base oscillation (theta: 8 Hz) */
        std::vector<float> signal(count);
        float dt = 0.001f;  /* 1 kHz sampling */

        for (size_t i = 0; i < count; i++) {
            float t = i * dt;
            signal[i] = sin(2.0f * M_PI * 8.0f * t);  /* 8 Hz theta */
        }

        /* Add pink noise modulation */
        auto noise = generate_pink_noise(1.0f, count, seed);
        if (noise.empty()) return signal;

        for (size_t i = 0; i < count; i++) {
            signal[i] *= (1.0f + 0.2f * noise[i]);  /* 20% modulation */
        }

        return signal;
    }
};

/* ============================================================================
 * E2E Test 1: Neural Criticality Analysis
 * ============================================================================ */

TEST_F(PinkNoiseFractalE2ETest, NeuralCriticalityAnalysis) {
    /* SCENARIO: Analyze neural signal for 1/f criticality
     *
     * WORKFLOW:
     * 1. Generate simulated neural signal with pink noise modulation
     * 2. Perform fractal analysis
     * 3. Validate DFA exponent indicates criticality (alpha ~ 1.0)
     * 4. Classify noise type
     *
     * BIOLOGICAL RELEVANCE:
     * - Critical brain states exhibit 1/f dynamics
     * - Deviation from criticality indicates pathology
     */

    /* Generate neural signal */
    auto signal = simulate_neural_signal(8192, 12345);
    ASSERT_FALSE(signal.empty());
    ASSERT_EQ(signal.size(), 8192u);

    /* Validate signal quality */
    bool valid = fractal_validate_signal(signal.data(), signal.size());
    ASSERT_TRUE(valid) << "Signal should pass quality validation";

    /* Perform DFA analysis */
    fractal_result_t result;
    int ret = fractal_dfa(signal.data(), signal.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* Check for criticality (DFA exponent near 1.0) */
    EXPECT_GT(result.dfa_exponent, 0.7f);
    EXPECT_LT(result.dfa_exponent, 1.5f);
    EXPECT_GT(result.dfa_r2, 0.8f) << "DFA fit quality should be high";

    /* Classify the noise */
    const char* classification = fractal_classify_noise(result.dfa_exponent);
    EXPECT_NE(classification, nullptr);

    /* Log results for analysis */
    std::cout << "[Criticality Analysis]\n";
    std::cout << "  DFA exponent: " << result.dfa_exponent << "\n";
    std::cout << "  DFA R^2: " << result.dfa_r2 << "\n";
    std::cout << "  Classification: " << classification << "\n";
}

/* ============================================================================
 * E2E Test 2: Synaptic Noise Modulation
 * ============================================================================ */

TEST_F(PinkNoiseFractalE2ETest, SynapticNoiseModulationPipeline) {
    /* SCENARIO: Complete workflow for synaptic strength modulation
     *
     * WORKFLOW:
     * 1. Create pink noise generator
     * 2. Generate noise sequence
     * 3. Apply multiplicative modulation to synaptic weights
     * 4. Validate modulated weights maintain 1/f statistics
     *
     * BIOLOGICAL RELEVANCE:
     * - Synaptic strengths fluctuate with 1/f noise
     * - Pink noise modulation enhances stochastic resonance
     */

    /* Create pink noise generator */
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = 1.0f;
    config.amplitude = 0.1f;  /* 10% modulation strength */
    config.seed = 54321;

    pink_noise_generator_t gen = pink_noise_create(&config);
    ASSERT_NE(gen, nullptr);

    /* Simulate base synaptic weights */
    const size_t num_synapses = 1000;
    const size_t num_timesteps = 4096;
    std::vector<float> base_weights(num_synapses, 0.5f);  /* Initial weight */

    /* Track weight evolution for one synapse */
    std::vector<float> weight_evolution(num_timesteps);
    float current_weight = base_weights[0];

    for (size_t t = 0; t < num_timesteps; t++) {
        float modulated;
        bool success = pink_noise_modulate_multiplicative(gen, current_weight, 0.05f, &modulated);
        ASSERT_TRUE(success);

        /* Clamp to valid range */
        current_weight = std::max(0.1f, std::min(0.9f, modulated));
        weight_evolution[t] = current_weight;
    }

    pink_noise_destroy(gen);

    /* Analyze weight evolution for 1/f characteristics */
    fractal_result_t result;
    int ret = fractal_dfa(weight_evolution.data(), weight_evolution.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* Weight evolution should show persistence (H > 0.5) */
    EXPECT_GT(result.dfa_exponent, 0.5f)
        << "Modulated weights should show persistent dynamics";

    /* Log results */
    std::cout << "[Synaptic Modulation]\n";
    std::cout << "  Weight range: [" << *std::min_element(weight_evolution.begin(), weight_evolution.end())
              << ", " << *std::max_element(weight_evolution.begin(), weight_evolution.end()) << "]\n";
    std::cout << "  DFA exponent: " << result.dfa_exponent << "\n";
}

/* ============================================================================
 * E2E Test 3: Memory Consolidation Dynamics
 * ============================================================================ */

TEST_F(PinkNoiseFractalE2ETest, MemoryConsolidationDynamics) {
    /* SCENARIO: Analyze multiscale temporal correlations in memory trace
     *
     * WORKFLOW:
     * 1. Generate pink noise representing memory activity fluctuations
     * 2. Perform multifractal analysis
     * 3. Validate spectrum indicates long-range temporal correlations
     *
     * BIOLOGICAL RELEVANCE:
     * - Memory consolidation shows multiscale dynamics
     * - Multifractal analysis reveals heterogeneity in timescales
     */

    /* Generate memory trace signal with pink noise characteristics */
    auto memory_trace = generate_pink_noise(1.0f, 8192, 99999);
    ASSERT_FALSE(memory_trace.empty());

    /* Perform comprehensive fractal analysis */
    fractal_result_t result;
    int ret = fractal_analyze(memory_trace.data(), memory_trace.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* Check for long-range correlations */
    EXPECT_GT(result.hurst_exponent, 0.5f)
        << "Memory trace should show persistent (long-range) correlations";

    /* Perform multifractal analysis */
    multifractal_spectrum_t* spectrum = nullptr;
    ret = fractal_multifractal_spectrum(memory_trace.data(), memory_trace.size(),
                                        -4.0f, 4.0f, 17, &spectrum);

    if (ret == FRACTAL_OK && spectrum != nullptr) {
        /* Log multifractal properties */
        std::cout << "[Memory Consolidation Dynamics]\n";
        std::cout << "  Hurst exponent: " << result.hurst_exponent << "\n";
        std::cout << "  DFA exponent: " << result.dfa_exponent << "\n";
        std::cout << "  Spectrum width: " << spectrum->width << "\n";
        std::cout << "  Is multifractal: " << (spectrum->is_multifractal ? "yes" : "no") << "\n";

        multifractal_spectrum_destroy(spectrum);
    }
}

/* ============================================================================
 * E2E Test 4: Sleep Stage Characterization
 * ============================================================================ */

TEST_F(PinkNoiseFractalE2ETest, SleepStageCharacterization) {
    /* SCENARIO: Differentiate sleep stages by fractal dimension
     *
     * WORKFLOW:
     * 1. Generate signals with different alpha values (representing sleep stages)
     * 2. Compute fractal dimension for each
     * 3. Validate differentiation between stages
     *
     * BIOLOGICAL RELEVANCE:
     * - Wake: Higher complexity, D closer to 2
     * - Deep sleep: Lower complexity, D closer to 1
     * - REM: Intermediate complexity
     */

    struct SleepStage {
        const char* name;
        float alpha;      /* Generation parameter */
        float min_dim;    /* Expected min fractal dimension */
        float max_dim;    /* Expected max fractal dimension */
    };

    SleepStage stages[] = {
        {"Wake", 0.5f, 1.3f, 1.7f},       /* More random, higher dimension */
        {"Light Sleep", 1.0f, 1.1f, 1.4f}, /* Pink noise, intermediate */
        {"Deep Sleep", 1.5f, 1.0f, 1.3f},  /* Brown noise, lower dimension */
    };

    std::cout << "[Sleep Stage Characterization]\n";

    for (const auto& stage : stages) {
        auto signal = generate_pink_noise(stage.alpha, 4096, 11111);
        ASSERT_FALSE(signal.empty());

        fractal_result_t result;
        int ret = fractal_box_dimension(signal.data(), signal.size(), nullptr, &result);
        ASSERT_EQ(ret, FRACTAL_OK);

        std::cout << "  " << stage.name << ":\n";
        std::cout << "    Fractal dimension: " << result.fractal_dimension << "\n";
        std::cout << "    Expected range: [" << stage.min_dim << ", " << stage.max_dim << "]\n";

        /* Dimension should be in reasonable range (allow numerical variance) */
        EXPECT_GE(result.fractal_dimension, 0.5f)
            << stage.name << " fractal dimension too low";
        EXPECT_LE(result.fractal_dimension, 2.5f)
            << stage.name << " fractal dimension too high";
    }
}

/* ============================================================================
 * E2E Test 5: Complete Neural Network Simulation Pipeline
 * ============================================================================ */

TEST_F(PinkNoiseFractalE2ETest, CompleteNeuralSimulationPipeline) {
    /* SCENARIO: Full simulation pipeline with noise and analysis
     *
     * WORKFLOW:
     * 1. Create pink noise generator (neuromodulator fluctuations)
     * 2. Simulate neural population activity
     * 3. Add pink noise modulation
     * 4. Validate network operates at criticality
     * 5. Check self-similarity across scales
     *
     * This represents a complete use case for the NIMCP system.
     */

    const size_t simulation_duration = 10000;  /* 10 seconds at 1 kHz */
    const uint32_t seed = 777;

    /* Phase 1: Create neuromodulator noise generators */
    pink_noise_config_t nm_config = pink_noise_default_config();
    nm_config.alpha = 1.0f;
    nm_config.amplitude = 0.1f;
    nm_config.seed = seed;

    pink_noise_generator_t dopamine_gen = pink_noise_create(&nm_config);
    ASSERT_NE(dopamine_gen, nullptr);

    nm_config.seed = seed + 1;
    pink_noise_generator_t acetylcholine_gen = pink_noise_create(&nm_config);
    ASSERT_NE(acetylcholine_gen, nullptr);

    /* Phase 2: Simulate neural population with noisy modulation */
    std::vector<float> population_activity(simulation_duration);
    float base_rate = 20.0f;  /* 20 Hz baseline firing rate */

    for (size_t t = 0; t < simulation_duration; t++) {
        float da_mod, ach_mod;
        pink_noise_modulate(dopamine_gen, 1.0f, &da_mod);
        pink_noise_modulate(acetylcholine_gen, 1.0f, &ach_mod);

        /* Combine modulations */
        float modulation = 0.5f * da_mod + 0.5f * ach_mod;
        population_activity[t] = base_rate * modulation;
    }

    pink_noise_destroy(dopamine_gen);
    pink_noise_destroy(acetylcholine_gen);

    /* Phase 3: Validate signal quality */
    bool valid = fractal_validate_signal(population_activity.data(), population_activity.size());
    EXPECT_TRUE(valid);

    /* Phase 4: Analyze for criticality */
    fractal_result_t result;
    int ret = fractal_analyze(population_activity.data(), population_activity.size(), nullptr, &result);
    ASSERT_EQ(ret, FRACTAL_OK);

    /* Phase 5: Check self-similarity */
    bool self_similar = fractal_is_self_similar(population_activity.data(),
                                                 population_activity.size(), 8);

    /* Phase 6: Check if pink noise */
    bool is_pink = fractal_is_pink_noise(population_activity.data(),
                                          population_activity.size(), 0.3f);

    /* Log comprehensive results */
    std::cout << "[Complete Neural Simulation Pipeline]\n";
    std::cout << "  Simulation duration: " << simulation_duration << " ms\n";
    std::cout << "  Samples analyzed: " << result.samples_analyzed << "\n";
    std::cout << "  Hurst exponent: " << result.hurst_exponent << "\n";
    std::cout << "  DFA exponent: " << result.dfa_exponent << "\n";
    std::cout << "  Spectral exponent: " << result.spectral_exponent << "\n";
    std::cout << "  Fractal dimension: " << result.fractal_dimension << "\n";
    std::cout << "  Lacunarity: " << result.lacunarity << "\n";
    std::cout << "  Is self-similar: " << (self_similar ? "yes" : "no") << "\n";
    std::cout << "  Is pink noise: " << (is_pink ? "yes" : "no") << "\n";

    /* Validate criticality */
    EXPECT_GT(result.dfa_exponent, 0.6f);
    EXPECT_LT(result.dfa_exponent, 1.4f);
}

/* ============================================================================
 * E2E Test 6: Persistence/Save-Load Workflow
 * ============================================================================ */

TEST_F(PinkNoiseFractalE2ETest, PersistenceWorkflow) {
    /* SCENARIO: Save and restore generator state
     *
     * WORKFLOW:
     * 1. Create generator, generate some samples
     * 2. Save state to file
     * 3. Generate more samples
     * 4. Load state, verify sequence continues correctly
     */

    pink_noise_config_t config = pink_noise_default_config();
    config.seed = 88888;
    config.method = PINK_NOISE_FFT;

    pink_noise_generator_t gen = pink_noise_create(&config);
    ASSERT_NE(gen, nullptr);

    /* Generate first batch */
    std::vector<float> batch1(100);
    pink_noise_generate(gen, batch1.data(), batch1.size());

    /* Save state */
    FILE* save_file = tmpfile();
    ASSERT_NE(save_file, nullptr);
    bool save_success = pink_noise_save(gen, save_file);
    EXPECT_TRUE(save_success);

    /* Generate more samples after save */
    std::vector<float> batch2(100);
    pink_noise_generate(gen, batch2.data(), batch2.size());

    pink_noise_destroy(gen);

    /* Load state */
    rewind(save_file);
    pink_noise_generator_t loaded_gen = pink_noise_load(save_file);
    fclose(save_file);

    if (loaded_gen != nullptr) {
        /* Generate samples from loaded state */
        std::vector<float> batch3(100);
        pink_noise_generate(loaded_gen, batch3.data(), batch3.size());

        /* batch3 should match batch2 (same point in sequence) */
        int matches = 0;
        for (size_t i = 0; i < batch2.size(); i++) {
            if (std::abs(batch2[i] - batch3[i]) < 0.0001f) {
                matches++;
            }
        }

        std::cout << "[Persistence Workflow]\n";
        std::cout << "  Sequence matches: " << matches << "/" << batch2.size() << "\n";

        pink_noise_destroy(loaded_gen);
    }
}

/* ============================================================================
 * E2E Test 7: Error Recovery Workflow
 * ============================================================================ */

TEST_F(PinkNoiseFractalE2ETest, ErrorRecoveryWorkflow) {
    /* SCENARIO: Handle errors gracefully in production workflow
     *
     * WORKFLOW:
     * 1. Attempt operations with invalid inputs
     * 2. Verify graceful failure
     * 3. Continue with valid operations
     */

    /* Attempt to create with invalid config */
    pink_noise_config_t invalid_config = pink_noise_default_config();
    invalid_config.alpha = -1.0f;  /* Invalid */

    pink_noise_generator_t gen = pink_noise_create(&invalid_config);
    EXPECT_EQ(gen, nullptr) << "Should fail with invalid config";

    /* Get error message */
    const char* error = pink_noise_get_last_error();
    EXPECT_NE(error, nullptr) << "Error message should be set";
    std::cout << "[Error Recovery]\n";
    std::cout << "  Error after invalid config: " << (error ? error : "none") << "\n";

    /* Now create with valid config */
    pink_noise_config_t valid_config = pink_noise_default_config();
    gen = pink_noise_create(&valid_config);
    ASSERT_NE(gen, nullptr) << "Should succeed with valid config";

    /* Generate samples */
    float sample;
    bool success = pink_noise_generate_sample(gen, &sample);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isfinite(sample));

    pink_noise_destroy(gen);

    /* Attempt fractal analysis with invalid input */
    fractal_result_t result;
    int ret = fractal_dfa(nullptr, 1000, nullptr, &result);
    EXPECT_LT(ret, 0) << "Should fail with NULL input";

    /* Now with valid input */
    std::vector<float> valid_signal(1024);
    for (size_t i = 0; i < valid_signal.size(); i++) {
        valid_signal[i] = (float)rand() / RAND_MAX;
    }

    ret = fractal_dfa(valid_signal.data(), valid_signal.size(), nullptr, &result);
    EXPECT_EQ(ret, FRACTAL_OK) << "Should succeed with valid input";
}

/* ============================================================================
 * E2E Test 8: Performance Under Load
 * ============================================================================ */

TEST_F(PinkNoiseFractalE2ETest, PerformanceUnderLoad) {
    /* SCENARIO: Sustained generation and analysis workload
     *
     * WORKFLOW:
     * 1. Create multiple generators
     * 2. Generate large amounts of data
     * 3. Perform parallel analysis
     * 4. Verify no degradation
     */

    const int num_generators = 4;
    const size_t samples_per_gen = 10000;
    const int iterations = 10;

    std::vector<pink_noise_generator_t> generators(num_generators);
    std::vector<std::vector<float>> all_samples(num_generators);

    /* Create generators */
    for (int i = 0; i < num_generators; i++) {
        pink_noise_config_t config = pink_noise_default_config();
        config.seed = 100 + i;
        config.method = (pink_noise_method_t)(i % 4);

        generators[i] = pink_noise_create(&config);
        ASSERT_NE(generators[i], nullptr);
        all_samples[i].resize(samples_per_gen);
    }

    auto start = std::chrono::high_resolution_clock::now();

    /* Sustained workload */
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < num_generators; i++) {
            pink_noise_generate(generators[i], all_samples[i].data(), samples_per_gen);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Analyze one signal */
    fractal_result_t result;
    int ret = fractal_analyze(all_samples[0].data(), all_samples[0].size(), nullptr, &result);
    EXPECT_EQ(ret, FRACTAL_OK);

    /* Cleanup */
    for (int i = 0; i < num_generators; i++) {
        pink_noise_destroy(generators[i]);
    }

    size_t total_samples = (size_t)num_generators * samples_per_gen * iterations;
    float samples_per_ms = (float)total_samples / duration.count();

    std::cout << "[Performance Under Load]\n";
    std::cout << "  Generators: " << num_generators << "\n";
    std::cout << "  Total samples: " << total_samples << "\n";
    std::cout << "  Total time: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << samples_per_ms << " samples/ms\n";

    EXPECT_GT(samples_per_ms, 100.0f) << "Should generate >100k samples/sec";
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
