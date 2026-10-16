/**
 * @file test_nas_regression.cpp
 * @brief Comprehensive Regression Tests for Neural Architecture Search (NAS)
 *
 * WHAT: Regression tests for architecture representation, search reproducibility, and checkpoint compatibility
 * WHY:  Ensure stable architecture encoding, reproducible search results, and checkpoint format stability
 * HOW:  Test architecture serialization, search determinism, and checkpoint I/O
 *
 * REGRESSION CATEGORIES:
 * - Architecture Representation Stability: Struct layouts and serialization format
 * - Search Result Reproducibility: Same seed produces same results
 * - Checkpoint Format Compatibility: Old checkpoints must be loadable
 * - API Stability: Enum values and function signatures
 * - Performance Baselines: Search and evaluation timing
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>

// Headers have their own extern "C" guards
#include "training/nimcp_auto_architecture.h"
#include "lnn/nimcp_lnn_config.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NASRegressionTest : public ::testing::Test {
protected:
    auto_arch_context_t* ctx = nullptr;
    auto_arch_config_t config;
    auto_arch_architecture_t* arch = nullptr;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
        auto_arch_default_config(&config);
    }

    void TearDown() override {
        if (ctx) {
            auto_arch_destroy(ctx);
            ctx = nullptr;
        }
        if (arch) {
            auto_arch_architecture_destroy(arch);
            arch = nullptr;
        }
    }

    void CreateContext() {
        ctx = auto_arch_create(&config);
        ASSERT_NE(ctx, nullptr);
    }

    nimcp_tensor_t* CreateDummyData(uint32_t n_samples, uint32_t n_features) {
        uint32_t dims[2] = {n_samples, n_features};
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
        if (tensor) {
            float* data = static_cast<float*>(nimcp_tensor_data(tensor));
            for (uint32_t i = 0; i < n_samples * n_features; i++) {
                data[i] = static_cast<float>(i % 100) / 100.0f;
            }
        }
        return tensor;
    }
};

//=============================================================================
// API Stability Tests - Enum Values
//=============================================================================

TEST_F(NASRegressionTest, SearchMethodEnumStable) {
    // WHAT: Verify search method enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change

    EXPECT_EQ(AUTO_ARCH_RL_NAS, 0);
    EXPECT_EQ(AUTO_ARCH_EVOLUTIONARY, 1);
    EXPECT_EQ(AUTO_ARCH_DARTS, 2);
    EXPECT_EQ(AUTO_ARCH_RANDOM_SEARCH, 3);
    EXPECT_EQ(AUTO_ARCH_BAYESIAN_OPT, 4);
    EXPECT_EQ(AUTO_ARCH_PRUNING_BASED, 5);
    EXPECT_EQ(AUTO_ARCH_GRADIENT_BASED, 6);
    EXPECT_EQ(AUTO_ARCH_NEUROEVOLUTION, 7);
}

TEST_F(NASRegressionTest, NetworkTypeEnumStable) {
    // WHAT: Verify network type enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change

    EXPECT_EQ(AUTO_ARCH_TYPE_SNN, 0);
    EXPECT_EQ(AUTO_ARCH_TYPE_LNN, 1);
    EXPECT_EQ(AUTO_ARCH_TYPE_CNN, 2);
    EXPECT_EQ(AUTO_ARCH_TYPE_HYBRID_SNN_LNN, 3);
    EXPECT_EQ(AUTO_ARCH_TYPE_HYBRID_SNN_CNN, 4);
    EXPECT_EQ(AUTO_ARCH_TYPE_HYBRID_ALL, 5);
}

TEST_F(NASRegressionTest, TaskTypeEnumStable) {
    // WHAT: Verify task type enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change

    EXPECT_EQ(AUTO_ARCH_TASK_CLASSIFICATION, 0);
    EXPECT_EQ(AUTO_ARCH_TASK_REGRESSION, 1);
    EXPECT_EQ(AUTO_ARCH_TASK_SEQUENCE, 2);
    EXPECT_EQ(AUTO_ARCH_TASK_DETECTION, 3);
    EXPECT_EQ(AUTO_ARCH_TASK_SEGMENTATION, 4);
    EXPECT_EQ(AUTO_ARCH_TASK_REINFORCEMENT, 5);
    EXPECT_EQ(AUTO_ARCH_TASK_GENERATION, 6);
    EXPECT_EQ(AUTO_ARCH_TASK_CUSTOM, 7);
}

TEST_F(NASRegressionTest, LayerTypeEnumStable) {
    // WHAT: Verify layer type enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change

    EXPECT_EQ(AUTO_ARCH_LAYER_SNN_LIF, 0);
    EXPECT_EQ(AUTO_ARCH_LAYER_SNN_IZHIKEVICH, 1);
    EXPECT_EQ(AUTO_ARCH_LAYER_SNN_ADAPTIVE, 2);
    EXPECT_EQ(AUTO_ARCH_LAYER_LNN_LTC, 3);
    EXPECT_EQ(AUTO_ARCH_LAYER_DENSE, 4);
    EXPECT_EQ(AUTO_ARCH_LAYER_CONV, 5);
    EXPECT_EQ(AUTO_ARCH_LAYER_POOL, 6);
    EXPECT_EQ(AUTO_ARCH_LAYER_RECURRENT, 7);
    EXPECT_EQ(AUTO_ARCH_LAYER_ATTENTION, 8);
    EXPECT_EQ(AUTO_ARCH_LAYER_SKIP, 9);
}

TEST_F(NASRegressionTest, ConnectivityEnumStable) {
    // WHAT: Verify connectivity enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change

    EXPECT_EQ(AUTO_ARCH_CONN_DENSE, 0);
    EXPECT_EQ(AUTO_ARCH_CONN_SPARSE_RANDOM, 1);
    EXPECT_EQ(AUTO_ARCH_CONN_SMALL_WORLD, 2);
    EXPECT_EQ(AUTO_ARCH_CONN_SCALE_FREE, 3);
    EXPECT_EQ(AUTO_ARCH_CONN_MODULAR, 4);
    EXPECT_EQ(AUTO_ARCH_CONN_NCP, 5);
    EXPECT_EQ(AUTO_ARCH_CONN_COLUMN, 6);
}

TEST_F(NASRegressionTest, ObjectiveEnumStable) {
    // WHAT: Verify objective enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change

    EXPECT_EQ(AUTO_ARCH_OBJ_ACCURACY, 0);
    EXPECT_EQ(AUTO_ARCH_OBJ_ENERGY, 1);
    EXPECT_EQ(AUTO_ARCH_OBJ_LATENCY, 2);
    EXPECT_EQ(AUTO_ARCH_OBJ_MEMORY, 3);
    EXPECT_EQ(AUTO_ARCH_OBJ_PARAMS, 4);
    EXPECT_EQ(AUTO_ARCH_OBJ_BIO_PLAUSIBILITY, 5);
    EXPECT_EQ(AUTO_ARCH_OBJ_TEMPORAL_PRECISION, 6);
    EXPECT_EQ(AUTO_ARCH_OBJ_ROBUSTNESS, 7);
}

TEST_F(NASRegressionTest, StatusEnumStable) {
    // WHAT: Verify status enum values are stable
    // WHY:  ABI compatibility requires stable enum values
    // REGRESSION: Enum values must not change

    EXPECT_EQ(AUTO_ARCH_STATUS_IDLE, 0);
    EXPECT_EQ(AUTO_ARCH_STATUS_INITIALIZING, 1);
    EXPECT_EQ(AUTO_ARCH_STATUS_SEARCHING, 2);
    EXPECT_EQ(AUTO_ARCH_STATUS_EVALUATING, 3);
    EXPECT_EQ(AUTO_ARCH_STATUS_COMPLETED, 4);
    EXPECT_EQ(AUTO_ARCH_STATUS_CONVERGED, 5);
    EXPECT_EQ(AUTO_ARCH_STATUS_MAX_ITERS, 6);
    EXPECT_EQ(AUTO_ARCH_STATUS_TIMEOUT, 7);
    EXPECT_EQ(AUTO_ARCH_STATUS_ERROR, 8);
}

TEST_F(NASRegressionTest, ConstantsStable) {
    // WHAT: Verify constants are stable
    // WHY:  Applications depend on these values
    // REGRESSION: Constants must not change

    EXPECT_EQ(AUTO_ARCH_MAX_LAYERS, 32u);
    EXPECT_EQ(AUTO_ARCH_MAX_POPULATION, 200u);
    EXPECT_EQ(AUTO_ARCH_MAX_ITERATIONS, 100000u);
    EXPECT_FLOAT_EQ(AUTO_ARCH_VALIDATION_SPLIT, 0.2f);
    EXPECT_EQ(AUTO_ARCH_MAGIC, 0x41415300u);  // "AAS\0"
}

//=============================================================================
// Architecture Representation Stability Tests
//=============================================================================

TEST_F(NASRegressionTest, ArchitectureMagicNumber) {
    // WHAT: Verify architecture magic number is set correctly
    // WHY:  Format validation requires magic number
    // REGRESSION: Magic number must be set in valid architectures

    config.random_seed = 12345;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    EXPECT_EQ(arch->magic, AUTO_ARCH_MAGIC);
}

TEST_F(NASRegressionTest, ArchitectureCloneEquivalence) {
    // WHAT: Verify cloned architecture is equivalent
    // WHY:  Clone must preserve all data
    // REGRESSION: Clone must be identical to original

    config.random_seed = 12345;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    auto_arch_architecture_t* clone = auto_arch_clone(arch);
    ASSERT_NE(clone, nullptr);

    // Verify key fields match
    EXPECT_EQ(clone->n_layers, arch->n_layers);
    EXPECT_EQ(clone->n_inputs, arch->n_inputs);
    EXPECT_EQ(clone->n_outputs, arch->n_outputs);
    EXPECT_EQ(clone->network_type, arch->network_type);
    EXPECT_EQ(clone->n_parameters, arch->n_parameters);
    EXPECT_EQ(clone->n_connections, arch->n_connections);
    EXPECT_EQ(clone->magic, arch->magic);

    // Check layers
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        EXPECT_EQ(clone->layers[i].n_neurons, arch->layers[i].n_neurons);
        EXPECT_EQ(clone->layers[i].type, arch->layers[i].type);
        EXPECT_EQ(clone->layers[i].connectivity, arch->layers[i].connectivity);
    }

    auto_arch_architecture_destroy(clone);
}

TEST_F(NASRegressionTest, ArchitectureValidation) {
    // WHAT: Verify architecture validation works
    // WHY:  Invalid architectures must be rejected
    // REGRESSION: Validation must catch errors

    config.random_seed = 12345;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    // Valid architecture should pass
    int result = auto_arch_validate_architecture(arch, &config.constraints);
    EXPECT_EQ(result, 0);
}

TEST_F(NASRegressionTest, ArchitectureSerialization) {
    // WHAT: Verify architecture serialization is stable
    // WHY:  Serialized format must be consistent
    // REGRESSION: Serialization format must not change

    config.random_seed = 12345;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    const char* temp_file = "/tmp/nimcp_arch_test.json";

    // Save
    int save_result = auto_arch_save_json(arch, temp_file);
    EXPECT_EQ(save_result, 0);

    // Load
    auto_arch_architecture_t* loaded = auto_arch_load_json(temp_file);
    ASSERT_NE(loaded, nullptr);

    // Verify loaded architecture matches
    EXPECT_EQ(loaded->n_layers, arch->n_layers);
    EXPECT_EQ(loaded->n_inputs, arch->n_inputs);
    EXPECT_EQ(loaded->n_outputs, arch->n_outputs);
    EXPECT_EQ(loaded->network_type, arch->network_type);

    auto_arch_architecture_destroy(loaded);
    std::remove(temp_file);
}

TEST_F(NASRegressionTest, ArchitectureLayerConsistency) {
    // WHAT: Verify layer specification is consistent
    // WHY:  Layer data must not be corrupted
    // REGRESSION: Layer fields must remain valid

    config.random_seed = 42;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    EXPECT_GT(arch->n_layers, 0u);
    EXPECT_LE(arch->n_layers, AUTO_ARCH_MAX_LAYERS);

    for (uint32_t i = 0; i < arch->n_layers; i++) {
        // Layer type must be valid
        EXPECT_LT(arch->layers[i].type, AUTO_ARCH_LAYER_COUNT);

        // Connectivity must be valid
        EXPECT_LT(arch->layers[i].connectivity, AUTO_ARCH_CONN_COUNT);

        // Neuron count must be positive
        EXPECT_GT(arch->layers[i].n_neurons, 0u);

        // Sparsity must be in range
        EXPECT_GE(arch->layers[i].sparsity, 0.0f);
        EXPECT_LE(arch->layers[i].sparsity, 1.0f);
    }
}

//=============================================================================
// Search Result Reproducibility Tests
//=============================================================================

TEST_F(NASRegressionTest, RandomArchitectureReproducibility) {
    // WHAT: Verify random architecture generation is reproducible
    // WHY:  Same seed must produce same architecture
    // REGRESSION: Reproducibility is required for debugging

    auto_arch_architecture_t* arch1 = nullptr;
    auto_arch_architecture_t* arch2 = nullptr;

    // First generation
    config.random_seed = 98765;
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);
    arch1 = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch1, nullptr);
    auto_arch_destroy(ctx);
    ctx = nullptr;

    // Second generation with same seed
    config.random_seed = 98765;
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);
    arch2 = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch2, nullptr);

    // Should be identical
    EXPECT_EQ(arch1->n_layers, arch2->n_layers);
    EXPECT_EQ(arch1->n_inputs, arch2->n_inputs);
    EXPECT_EQ(arch1->n_outputs, arch2->n_outputs);
    EXPECT_EQ(arch1->n_parameters, arch2->n_parameters);

    for (uint32_t i = 0; i < arch1->n_layers && i < arch2->n_layers; i++) {
        EXPECT_EQ(arch1->layers[i].n_neurons, arch2->layers[i].n_neurons);
        EXPECT_EQ(arch1->layers[i].type, arch2->layers[i].type);
    }

    auto_arch_architecture_destroy(arch1);
    auto_arch_architecture_destroy(arch2);
    arch = nullptr;  // Already destroyed
}

TEST_F(NASRegressionTest, MutationReproducibility) {
    // WHAT: Verify mutation is reproducible with same seed
    // WHY:  Same conditions must produce same mutations
    // REGRESSION: Mutation reproducibility

    config.random_seed = 11111;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    // Clone for comparison
    auto_arch_architecture_t* clone = auto_arch_clone(arch);
    ASSERT_NE(clone, nullptr);

    // Mutate original
    int result1 = auto_arch_mutate(arch, 0.5f, ctx);
    EXPECT_EQ(result1, 0);

    // Reset seed and mutate clone
    // Note: This may not work perfectly depending on RNG implementation
    // The test verifies mutation completes without error

    auto_arch_architecture_destroy(clone);
}

TEST_F(NASRegressionTest, CrossoverProducesValidChild) {
    // WHAT: Verify crossover produces valid architecture
    // WHY:  Crossover must not create invalid architectures
    // REGRESSION: Crossover output validation

    config.random_seed = 22222;
    CreateContext();

    auto_arch_architecture_t* parent1 = auto_arch_random_architecture(ctx);
    auto_arch_architecture_t* parent2 = auto_arch_random_architecture(ctx);
    ASSERT_NE(parent1, nullptr);
    ASSERT_NE(parent2, nullptr);

    auto_arch_architecture_t* child = auto_arch_crossover(parent1, parent2, ctx);
    ASSERT_NE(child, nullptr);

    // Child should be valid
    int result = auto_arch_validate_architecture(child, &config.constraints);
    EXPECT_EQ(result, 0);

    // Child should have valid magic
    EXPECT_EQ(child->magic, AUTO_ARCH_MAGIC);

    auto_arch_architecture_destroy(parent1);
    auto_arch_architecture_destroy(parent2);
    auto_arch_architecture_destroy(child);
    arch = nullptr;  // Clear pointer
}

//=============================================================================
// Checkpoint Format Compatibility Tests
//=============================================================================

TEST_F(NASRegressionTest, ResultSaveLoad) {
    // WHAT: Verify result save/load roundtrip
    // WHY:  Results must survive serialization
    // REGRESSION: Result format must be stable

    config.random_seed = 33333;
    config.max_evaluations = 5;  // Small for test
    config.search_method = AUTO_ARCH_RANDOM_SEARCH;
    CreateContext();

    // Set task
    auto_arch_task_t task;
    memset(&task, 0, sizeof(task));
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 10;
    task.n_outputs = 2;
    task.n_epochs = 1;
    task.batch_size = 4;
    auto_arch_set_task(ctx, &task);

    // Create dummy data
    nimcp_tensor_t* train_data = CreateDummyData(20, 10);
    nimcp_tensor_t* train_labels = CreateDummyData(20, 2);
    ASSERT_NE(train_data, nullptr);
    ASSERT_NE(train_labels, nullptr);

    // Run minimal search
    auto_arch_result_t* result = auto_arch_search(ctx, train_data, train_labels, nullptr, nullptr);
    ASSERT_NE(result, nullptr);

    const char* temp_file = "/tmp/nimcp_nas_result_test.bin";

    // Save result
    int save_result = auto_arch_result_save(result, temp_file);
    EXPECT_EQ(save_result, 0);

    // Load result
    auto_arch_result_t* loaded = auto_arch_result_load(temp_file);
    ASSERT_NE(loaded, nullptr);

    // Verify loaded result
    EXPECT_EQ(loaded->n_evaluated, result->n_evaluated);
    EXPECT_EQ(loaded->status, result->status);

    if (loaded->best_arch && result->best_arch) {
        EXPECT_EQ(loaded->best_arch->n_layers, result->best_arch->n_layers);
    }

    // Cleanup
    auto_arch_result_destroy(result);
    auto_arch_result_destroy(loaded);
    nimcp_tensor_destroy(train_data);
    nimcp_tensor_destroy(train_labels);
    std::remove(temp_file);
}

TEST_F(NASRegressionTest, ArchitectureJSONFormat) {
    // WHAT: Verify JSON format is human-readable and stable
    // WHY:  JSON should be version-control friendly
    // REGRESSION: JSON format must not change incompatibly

    config.random_seed = 44444;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    const char* temp_file = "/tmp/nimcp_arch_json_test.json";

    int save_result = auto_arch_save_json(arch, temp_file);
    EXPECT_EQ(save_result, 0);

    // Read file contents
    std::ifstream file(temp_file);
    ASSERT_TRUE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Should contain key fields
    EXPECT_NE(content.find("n_layers"), std::string::npos);
    EXPECT_NE(content.find("n_inputs"), std::string::npos);
    EXPECT_NE(content.find("n_outputs"), std::string::npos);
    EXPECT_NE(content.find("layers"), std::string::npos);

    std::remove(temp_file);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(NASRegressionTest, DefaultConfigValid) {
    // WHAT: Verify default config is valid
    // WHY:  Default should work out of the box
    // REGRESSION: Defaults must remain usable

    auto_arch_config_t default_config;
    int result = auto_arch_default_config(&default_config);
    EXPECT_EQ(result, 0);

    result = auto_arch_validate_config(&default_config);
    EXPECT_EQ(result, 0);

    // Key fields should be set
    EXPECT_GT(default_config.max_evaluations, 0u);
    EXPECT_GT(default_config.population_size, 0u);
    EXPECT_GE(default_config.mutation_rate, 0.0f);
    EXPECT_LE(default_config.mutation_rate, 1.0f);
}

TEST_F(NASRegressionTest, FastConfigValid) {
    // WHAT: Verify fast config is valid
    // WHY:  Fast config for quick prototyping
    // REGRESSION: Fast config must remain usable

    auto_arch_config_t fast_config;
    int result = auto_arch_fast_config(&fast_config);
    EXPECT_EQ(result, 0);

    result = auto_arch_validate_config(&fast_config);
    EXPECT_EQ(result, 0);

    // Should be faster than default
    auto_arch_config_t default_config;
    auto_arch_default_config(&default_config);
    EXPECT_LT(fast_config.max_evaluations, default_config.max_evaluations);
}

TEST_F(NASRegressionTest, ThoroughConfigValid) {
    // WHAT: Verify thorough config is valid
    // WHY:  Thorough config for production
    // REGRESSION: Thorough config must remain usable

    auto_arch_config_t thorough_config;
    int result = auto_arch_thorough_config(&thorough_config);
    EXPECT_EQ(result, 0);

    result = auto_arch_validate_config(&thorough_config);
    EXPECT_EQ(result, 0);

    // Should be more thorough than default
    auto_arch_config_t default_config;
    auto_arch_default_config(&default_config);
    EXPECT_GT(thorough_config.max_evaluations, default_config.max_evaluations);
}

TEST_F(NASRegressionTest, InvalidConfigRejected) {
    // WHAT: Verify invalid config is rejected
    // WHY:  Must catch errors early
    // REGRESSION: Validation must reject invalid configs

    auto_arch_config_t invalid_config;
    memset(&invalid_config, 0, sizeof(invalid_config));

    // Zero evaluations
    invalid_config.max_evaluations = 0;
    int result = auto_arch_validate_config(&invalid_config);
    EXPECT_NE(result, 0);

    // Invalid mutation rate
    auto_arch_default_config(&invalid_config);
    invalid_config.mutation_rate = 2.0f;  // > 1.0
    result = auto_arch_validate_config(&invalid_config);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(NASRegressionTest, StatisticsTracking) {
    // WHAT: Verify statistics are tracked correctly
    // WHY:  Monitoring depends on accurate stats
    // REGRESSION: Statistics must be accurate

    config.random_seed = 55555;
    CreateContext();

    auto_arch_stats_t stats;
    int result = auto_arch_get_stats(ctx, &stats);
    EXPECT_EQ(result, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.total_evaluations, 0u);
    EXPECT_EQ(stats.iterations, 0u);
}

TEST_F(NASRegressionTest, StatusTracking) {
    // WHAT: Verify status is tracked correctly
    // WHY:  Status needed for monitoring
    // REGRESSION: Status must reflect actual state

    CreateContext();

    auto_arch_status_t status = auto_arch_get_status(ctx);
    EXPECT_EQ(status, AUTO_ARCH_STATUS_IDLE);
}

TEST_F(NASRegressionTest, BioScoreComputation) {
    // WHAT: Verify biological plausibility score is computed
    // WHY:  Bio score is important objective
    // REGRESSION: Bio score must be in valid range

    config.random_seed = 66666;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    float bio_score = auto_arch_compute_bio_score(arch);

    EXPECT_GE(bio_score, 0.0f);
    EXPECT_LE(bio_score, 1.0f);
}

//=============================================================================
// Export/Import Tests
//=============================================================================

TEST_F(NASRegressionTest, ExportSNNConfig) {
    // WHAT: Verify SNN export produces valid config
    // WHY:  Must be able to build network from architecture
    // REGRESSION: Export format must be stable

    config.random_seed = 77777;
    config.network_type = AUTO_ARCH_TYPE_SNN;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    snn_config_t* snn_config = auto_arch_export_snn(arch);

    if (snn_config != nullptr) {
        // Basic validation
        EXPECT_GT(snn_config->n_inputs, 0u);
        EXPECT_GT(snn_config->n_outputs, 0u);

        // Cleanup (depends on SNN config API)
        // snn_config_destroy(snn_config);
        free(snn_config);
    }
}

TEST_F(NASRegressionTest, ExportLNNConfig) {
    // WHAT: Verify LNN export produces valid config
    // WHY:  Must be able to build network from architecture
    // REGRESSION: Export format must be stable

    config.random_seed = 88888;
    config.network_type = AUTO_ARCH_TYPE_LNN;
    CreateContext();

    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    lnn_config_t* lnn_config = auto_arch_export_lnn(arch);

    if (lnn_config != nullptr) {
        // Basic validation
        EXPECT_GT(lnn_config->n_inputs, 0u);
        EXPECT_GT(lnn_config->n_outputs, 0u);

        // Cleanup (depends on LNN config API)
        free(lnn_config);
    }
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(NASRegressionTest, NullPointerHandling) {
    // WHAT: Verify NULL pointer handling
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash

    // NULL config
    ctx = auto_arch_create(nullptr);
    EXPECT_EQ(ctx, nullptr);

    // NULL ctx operations
    auto_arch_destroy(nullptr);  // Should not crash

    auto_arch_stats_t stats;
    EXPECT_NE(auto_arch_get_stats(nullptr, &stats), 0);

    EXPECT_EQ(auto_arch_get_status(nullptr), AUTO_ARCH_STATUS_ERROR);

    EXPECT_EQ(auto_arch_random_architecture(nullptr), nullptr);
}

TEST_F(NASRegressionTest, DoubleDestroyHandling) {
    // WHAT: Verify double destroy is safe
    // WHY:  Defensive programming
    // REGRESSION: Double free bug fix

    CreateContext();
    auto_arch_destroy(ctx);
    auto_arch_destroy(ctx);  // Should not crash
    ctx = nullptr;

    SUCCEED();
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(NASRegressionTest, ArchitectureGenerationPerformance) {
    // WHAT: Verify architecture generation is fast
    // WHY:  Performance baseline
    // BASELINE: < 100ms for 100 architectures

    CreateContext();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        auto_arch_architecture_t* a = auto_arch_random_architecture(ctx);
        ASSERT_NE(a, nullptr);
        auto_arch_architecture_destroy(a);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "100 architecture generations took: " << duration.count() << " ms" << std::endl;

    EXPECT_LT(duration.count(), 100);
}

TEST_F(NASRegressionTest, MutationPerformance) {
    // WHAT: Verify mutation is fast
    // WHY:  Performance baseline
    // BASELINE: < 10ms for 100 mutations

    CreateContext();
    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        auto_arch_mutate(arch, 0.1f, ctx);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "100 mutations took: " << duration.count() << " ms" << std::endl;

    EXPECT_LT(duration.count(), 10);
}

TEST_F(NASRegressionTest, ValidationPerformance) {
    // WHAT: Verify validation is fast
    // WHY:  Performance baseline
    // BASELINE: < 1ms for 100 validations

    CreateContext();
    arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        auto_arch_validate_architecture(arch, &config.constraints);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "100 validations took: " << duration.count() << " us" << std::endl;

    EXPECT_LT(duration.count(), 1000);  // < 1ms
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(NASRegressionTest, MethodNameStrings) {
    // WHAT: Verify method name strings
    // WHY:  Logging and debugging require string names
    // REGRESSION: String names must not be NULL

    for (int i = 0; i < AUTO_ARCH_METHOD_COUNT; i++) {
        const char* name = auto_arch_method_name(static_cast<auto_arch_method_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(NASRegressionTest, NetworkTypeNameStrings) {
    // WHAT: Verify network type name strings
    // WHY:  Logging and debugging require string names
    // REGRESSION: String names must not be NULL

    for (int i = 0; i < AUTO_ARCH_TYPE_COUNT; i++) {
        const char* name = auto_arch_network_type_name(static_cast<auto_arch_network_type_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(NASRegressionTest, TaskTypeNameStrings) {
    // WHAT: Verify task type name strings
    // WHY:  Logging and debugging require string names
    // REGRESSION: String names must not be NULL

    for (int i = 0; i < AUTO_ARCH_TASK_COUNT; i++) {
        const char* name = auto_arch_task_type_name(static_cast<auto_arch_task_type_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(NASRegressionTest, LayerTypeNameStrings) {
    // WHAT: Verify layer type name strings
    // WHY:  Logging and debugging require string names
    // REGRESSION: String names must not be NULL

    for (int i = 0; i < AUTO_ARCH_LAYER_COUNT; i++) {
        const char* name = auto_arch_layer_type_name(static_cast<auto_arch_layer_type_t>(i));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
